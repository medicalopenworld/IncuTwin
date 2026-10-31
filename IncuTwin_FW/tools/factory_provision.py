#!/usr/bin/env python3
"""
Serializacion de IncuTwin en fabricacion (spec fleet-tools).

Por cada panel conectado por USB:
  1. Lee la MAC eFuse (esptool).
  2. Genera el serie ITW-YYWW-NNNN (o usa --sn) y el client id MQTT incutwin-YYWW-NNNN.
  3. Genera (o recupera de tools/factory/secrets/<SN>.json) una contrasena de 24 caracteres.
  4. Da de alta el cliente en el broker (dynamic-security por MQTT con el usuario admin:
     createClient + rol incutwin, cuyas ACL usan %c = client id), salvo --no-broker.
     Necesita MQTT_ADMIN_PASS en broker.env y que tools/broker_bootstrap.py haya creado el rol.
  5. Genera y flashea en 0x9000 la particion NVS con los namespaces factory (sn, hwrev, batch)
     y mqtt (user, pass).
  6. Crea la etiqueta PNG (QR = serie, 50x25 mm, 300 dpi) y apunta el panel en
     tools/factory/devices.csv (sin contrasena).

Ejecutar desde un entorno ESP-IDF (esptool y esp_idf_nvs_partition_gen). pip install paho-mqtt;
etiqueta: pip install qrcode pillow.

Uso:
  python tools/factory_provision.py --port COM12 [--hwrev 1.2] [--sn ITW-2640-0007] [--no-broker]
"""
import argparse
import csv
import datetime as dt
import json
import os
import secrets
import string
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from broker_mqtt import SECRETS_DIR, admin, client_id_for, dynsec_ok, ensure_client_role, load_env  # noqa: E402

FACTORY_DIR = os.path.join(HERE, "factory")
MANIFEST = os.path.join(FACTORY_DIR, "devices.csv")
NVS_OFFSET = "0x9000"
NVS_SIZE = 0x5000
LABEL_W_MM, LABEL_H_MM, DPI = 50, 25, 300
PASSWORD_LEN = 24
MQTT_ROLE = "incutwin"


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"ERROR: {' '.join(str(c) for c in cmd)}\n{r.stdout}\n{r.stderr}")
    return r.stdout


def read_mac(port):
    out = run([sys.executable, "-m", "esptool", "--port", port, "read-mac"])
    for line in out.splitlines():
        if "MAC:" in line:
            return line.split("MAC:")[1].strip().replace(":", "").upper()
    sys.exit("No se pudo leer la MAC")


def next_seq():
    if not os.path.exists(MANIFEST):
        return 1
    with open(MANIFEST, encoding="utf-8") as f:
        return sum(1 for _ in f)  # la cabecera cuenta como 0 -> primer panel = 1


def gen_sn(seq):
    now = dt.date.today()
    return f"ITW-{now.strftime('%y')}{now.isocalendar()[1]:02d}-{seq:04d}"


def gen_password():
    alphabet = string.ascii_letters + string.digits
    return "".join(secrets.choice(alphabet) for _ in range(PASSWORD_LEN))


def load_or_create_secret(sn, user):
    os.makedirs(SECRETS_DIR, exist_ok=True)
    path = os.path.join(SECRETS_DIR, f"{sn}.json")
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        if data.get("user") != user:
            sys.exit(f"{path} tiene user={data.get('user')!r}, esperado {user!r}")
        return data["pass"], False
    pwd = gen_password()
    with open(path, "w", encoding="utf-8") as f:
        json.dump({"user": user, "pass": pwd}, f, indent=2)
    return pwd, True


def broker_register(env, cafile, user, pwd):
    with admin(env, cafile) as b:
        if any(r.get("error") for r in b.dynsec([{"command": "getRole", "rolename": MQTT_ROLE}])):
            sys.exit(f"el rol {MQTT_ROLE} no existe en el broker: ejecuta tools/broker_bootstrap.py")
        exists = any(not r.get("error") for r in b.dynsec([{"command": "getClient", "username": user}]))
        if exists:
            dynsec_ok(b.dynsec([{"command": "setClientPassword", "username": user, "password": pwd}]))
            print(f"broker: {user} ya existia; contrasena actualizada")
        else:
            dynsec_ok(b.dynsec([{"command": "createClient", "username": user, "password": pwd}]))
            print(f"broker: cliente {user} creado")
        # Desde Mosquitto 2.1.2 (4-oct-2026) las sustituciones %c del rol incutwin funcionan, asi que
        # el panel solo necesita ese rol (los roles literales panel-<client_id> eran un parche
        # para el bug de la 2.0.22). El rol incubator-<id> lo asigna pair.py.
        ensure_client_role(b, user, MQTT_ROLE)
        print(f"broker: rol {MQTT_ROLE} asignado")


def build_nvs(sn, hwrev, batch, user, pwd, out_bin):
    csv_path = out_bin.replace(".bin", ".csv")
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        f.write("key,type,encoding,value\n")
        f.write("factory,namespace,,\n")
        f.write(f"sn,data,string,{sn}\n")
        f.write(f"hwrev,data,string,{hwrev}\n")
        f.write(f"batch,data,string,{batch}\n")
        f.write("mqtt,namespace,,\n")
        f.write(f"user,data,string,{user}\n")
        f.write(f"pass,data,string,{pwd}\n")
    run([sys.executable, "-m", "esp_idf_nvs_partition_gen", "generate", csv_path, out_bin, hex(NVS_SIZE)])
    os.remove(csv_path)  # el CSV lleva la contrasena en claro: no dejarlo por ahi
    return out_bin


def flash_nvs(port, bin_path):
    run([sys.executable, "-m", "esptool", "--port", port, "--chip", "esp32s3", "write-flash", NVS_OFFSET, bin_path])


def make_label(sn, mac, out_png):
    try:
        import qrcode
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        print("aviso: sin qrcode/pillow, no se genera etiqueta (pip install qrcode pillow)")
        return None
    w = int(LABEL_W_MM / 25.4 * DPI)
    h = int(LABEL_H_MM / 25.4 * DPI)
    img = Image.new("L", (w, h), 255)
    qr = qrcode.QRCode(border=1, box_size=10, error_correction=qrcode.constants.ERROR_CORRECT_M)
    qr.add_data(sn)
    qr.make(fit=True)
    qimg = qr.make_image(fill_color="black", back_color="white").convert("L")
    qsize = h - 20
    img.paste(qimg.resize((qsize, qsize), Image.NEAREST), (10, 10))
    d = ImageDraw.Draw(img)
    try:
        f_big = ImageFont.truetype("DejaVuSans-Bold.ttf", 44)
        f_small = ImageFont.truetype("DejaVuSans.ttf", 28)
    except OSError:
        f_big = f_small = ImageFont.load_default()
    x = qsize + 30
    d.text((x, h // 2 - 60), "IncuTwin", font=f_big, fill=0)
    d.text((x, h // 2), sn, font=f_small, fill=0)
    d.text((x, h // 2 + 40), f"MAC {mac}", font=f_small, fill=0)
    img.save(out_png, dpi=(DPI, DPI))
    return out_png


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True)
    ap.add_argument("--hwrev", default="1.2")
    ap.add_argument("--batch", default=dt.date.today().strftime("%Y%m"))
    ap.add_argument("--sn", help="forzar serie (por defecto se genera)")
    ap.add_argument("--no-broker", action="store_true", help="no dar de alta en el broker")
    ap.add_argument("--cafile", help="CA alternativa para TLS (por defecto la del sistema)")
    args = ap.parse_args()

    os.makedirs(FACTORY_DIR, exist_ok=True)
    env = load_env()

    mac = read_mac(args.port)
    sn = args.sn or gen_sn(next_seq())
    user = client_id_for(sn)
    pwd, created = load_or_create_secret(sn, user)
    print(f"MAC {mac} -> SN {sn} -> client id {user} ({'contrasena nueva' if created else 'contrasena existente'})")

    if not args.no_broker:
        broker_register(env, args.cafile, user, pwd)

    nvs_bin = os.path.join(SECRETS_DIR, f"nvs_{sn}.bin")  # contiene la contrasena: en secrets/
    build_nvs(sn, args.hwrev, args.batch, user, pwd, nvs_bin)
    flash_nvs(args.port, nvs_bin)
    print("NVS de fabrica flasheada en 0x9000")

    label = make_label(sn, mac, os.path.join(FACTORY_DIR, f"label_{sn}.png"))
    if label:
        print(f"Etiqueta: {label}")

    new_file = not os.path.exists(MANIFEST)
    with open(MANIFEST, "a", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        if new_file:
            w.writerow(["sn", "mac", "hwrev", "batch", "client_id", "date"])
        w.writerow([sn, mac, args.hwrev, args.batch, user, dt.datetime.now().isoformat(timespec="seconds")])
    print("Registrado en devices.csv")


if __name__ == "__main__":
    main()
