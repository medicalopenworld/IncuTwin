#!/usr/bin/env python3
"""
Serialización de IncuTwin en fabricación.

Por cada panel conectado por USB:
  1. Lee la MAC eFuse (esptool).
  2. Genera el número de serie:  ITW-YYWW-NNNN  (año+semana, secuencial).
  3. Crea y flashea la partición NVS de fábrica (namespace "factory":
     sn, hwrev, batch) en 0x9000.
  4. Genera la etiqueta PNG (QR + SN) lista para imprimir (50x25 mm, 300 dpi).
  5. Registra el dispositivo en tools/factory/devices.csv (manifiesto).

Requisitos:
  pip install esptool esp-idf-nvs-partition-gen qrcode pillow

Uso:
  python tools/factory_provision.py --port COM5 [--hwrev 1.2] [--sn ITW-...]

El firmware se flashea aparte con `pio run -t upload`. Este script solo
escribe la NVS, así que puede ejecutarse antes o después.
"""
import argparse
import csv
import datetime as dt
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FACTORY_DIR = os.path.join(HERE, "factory")
MANIFEST = os.path.join(FACTORY_DIR, "devices.csv")
NVS_OFFSET = "0x9000"
NVS_SIZE = 0x5000

LABEL_W_MM, LABEL_H_MM, DPI = 50, 25, 300


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"ERROR: {' '.join(cmd)}\n{r.stdout}\n{r.stderr}")
    return r.stdout


def read_mac(port):
    out = run([sys.executable, "-m", "esptool", "--port", port, "read_mac"])
    for line in out.splitlines():
        if "MAC:" in line:
            return line.split("MAC:")[1].strip().replace(":", "").upper()
    sys.exit("No se pudo leer la MAC")


def next_seq():
    if not os.path.exists(MANIFEST):
        return 1
    with open(MANIFEST) as f:
        return sum(1 for _ in f)  # header counts as 0 -> first device = 1


def gen_sn(seq):
    now = dt.date.today()
    yy = now.strftime("%y")
    ww = now.isocalendar()[1]
    return f"ITW-{yy}{ww:02d}-{seq:04d}"


def build_nvs(sn, hwrev, batch, out_bin):
    csv_path = out_bin.replace(".bin", ".csv")
    with open(csv_path, "w", newline="") as f:
        f.write("key,type,encoding,value\n")
        f.write("factory,namespace,,\n")
        f.write(f"sn,data,string,{sn}\n")
        f.write(f"hwrev,data,string,{hwrev}\n")
        f.write(f"batch,data,string,{batch}\n")
    run([sys.executable, "-m", "esp_idf_nvs_partition_gen", "generate",
         csv_path, out_bin, hex(NVS_SIZE)])
    return out_bin


def flash_nvs(port, bin_path):
    run([sys.executable, "-m", "esptool", "--port", port, "--chip", "esp32s3",
         "write_flash", NVS_OFFSET, bin_path])


def make_label(sn, mac, out_png):
    import qrcode
    from PIL import Image, ImageDraw, ImageFont

    w = int(LABEL_W_MM / 25.4 * DPI)
    h = int(LABEL_H_MM / 25.4 * DPI)
    img = Image.new("L", (w, h), 255)

    qr = qrcode.QRCode(border=1, box_size=10,
                       error_correction=qrcode.constants.ERROR_CORRECT_M)
    qr.add_data(sn)
    qr.make(fit=True)
    qimg = qr.make_image(fill_color="black", back_color="white").convert("L")
    qsize = h - 20
    qimg = qimg.resize((qsize, qsize), Image.NEAREST)
    img.paste(qimg, (10, 10))

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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--hwrev", default="1.2")
    ap.add_argument("--batch", default=dt.date.today().strftime("%Y%m"))
    ap.add_argument("--sn", help="forzar SN (por defecto se genera)")
    args = ap.parse_args()

    os.makedirs(FACTORY_DIR, exist_ok=True)

    mac = read_mac(args.port)
    sn = args.sn or gen_sn(next_seq())
    print(f"MAC {mac} -> SN {sn}")

    nvs_bin = os.path.join(FACTORY_DIR, f"nvs_{sn}.bin")
    build_nvs(sn, args.hwrev, args.batch, nvs_bin)
    flash_nvs(args.port, nvs_bin)
    print("NVS de fábrica flasheada")

    label = os.path.join(FACTORY_DIR, f"label_{sn}.png")
    make_label(sn, mac, label)
    print(f"Etiqueta: {label}")

    new_file = not os.path.exists(MANIFEST)
    with open(MANIFEST, "a", newline="") as f:
        wcsv = csv.writer(f)
        if new_file:
            wcsv.writerow(["sn", "mac", "hwrev", "batch", "date"])
        wcsv.writerow([sn, mac, args.hwrev, args.batch,
                       dt.datetime.now().isoformat(timespec="seconds")])
    print("Registrado en devices.csv")


if __name__ == "__main__":
    main()
