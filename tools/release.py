#!/usr/bin/env python3
"""Release del firmware IncuTwin (spec fleet-tools, ota-update).

  python tools/release.py [--no-build] [--upload user@host:/srv/fw/incutwin/] [--base-url URL]
                          [--target <SN|client_id>|all] [--publish] [--cafile ca.pem]

1. idf.py build (salvo --no-build) y comprueba el tamano frente al slot de 4 MB.
2. Copia build/incutwin.bin a dist/incutwin-<version>.bin y calcula su sha256.
3. Escribe dist/incutwin-<version>.json = {"url": "<base-url>/incutwin-<version>.bin", "sha256": "..."}.
4. --upload: scp del .bin al destino.
5. Imprime el mosquitto_pub listo para lanzar la OTA; con --publish y --target lo ejecuta
   (rol publisher de broker.env).

Ejecutar desde un entorno ESP-IDF.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DIST = os.path.join(ROOT, "dist")
SLOT_BYTES = 0x400000
DEFAULT_BASE_URL = "https://fw.medicalopenworld.org/incutwin"


def version():
    with open(os.path.join(ROOT, "version.txt"), encoding="utf-8") as f:
        return f.read().strip()


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--upload", help="destino scp, p. ej. deploy@fw.medicalopenworld.org:/srv/fw/incutwin/")
    ap.add_argument("--base-url", default=DEFAULT_BASE_URL)
    ap.add_argument("--target", help="SN, client id o 'all' para el mosquitto_pub")
    ap.add_argument("--publish", action="store_true", help="ejecutar el mosquitto_pub (rol publisher)")
    ap.add_argument("--cafile")
    args = ap.parse_args()

    ver = version()
    if not args.no_build:
        r = subprocess.run(["idf.py", "build"], cwd=ROOT, shell=(os.name == "nt"))
        if r.returncode != 0:
            sys.exit("idf.py build fallo")
    src = os.path.join(ROOT, "build", "incutwin.bin")
    if not os.path.exists(src):
        sys.exit(f"no existe {src}")
    size = os.path.getsize(src)
    if size > SLOT_BYTES - (1 << 20):
        print(f"AVISO: binario de {size} B deja menos de 1 MB de margen en el slot de 4 MB")

    os.makedirs(DIST, exist_ok=True)
    name = f"incutwin-{ver}.bin"
    dst = os.path.join(DIST, name)
    shutil.copyfile(src, dst)
    digest = sha256_of(dst)
    url = f"{args.base_url.rstrip('/')}/{name}"
    cmd_payload = {"url": url, "sha256": digest}
    with open(os.path.join(DIST, f"incutwin-{ver}.json"), "w", encoding="utf-8") as f:
        json.dump(cmd_payload, f, indent=2)
    print(f"{dst}  {size} B  sha256 {digest}")

    if args.upload:
        subprocess.run(["scp", dst, args.upload], check=True)
        print(f"subido a {args.upload}")

    payload = json.dumps(cmd_payload)
    target = args.target or "<client_id|all>"
    from broker_common import client_id_for  # noqa: E402
    cid = target if target in ("all", "<client_id|all>") else client_id_for(target)
    topic = f"incutwin/{cid}/cmd/ota"
    print("\nPara lanzar la OTA (rol publisher):")
    print(f"  mosquitto_pub -h mqtt.medicalopenworld.org -p 8883 --tls-use-os-certs -u publisher -P $MQTT_PUB_PASS "
          f"-i publisher -q 1 -t '{topic}' -m '{payload}'")
    if args.publish:
        if not args.target:
            sys.exit("--publish necesita --target")
        from broker_common import load_env, pub  # noqa: E402
        pub(load_env(), topic, payload, retain=False, cafile=args.cafile)
        print(f"OTA publicada en {topic}")


if __name__ == "__main__":
    main()
