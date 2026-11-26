#!/usr/bin/env python3
"""Captura la pantalla de una IncuTwin por la consola serie (comando `pantalla`) y la guarda en PNG.

Abre el puerto SIN resetear la placa (DTR/RTS bajados antes de abrir), envia antes los comandos que
se pidan (p. ej. `tienda gorros nina`), espera a que se pinten y pide la captura. Compone la
pantalla (RGB565) con la capa superior (ARGB8888: barra de navegacion y dialogos).

Uso:
  python tools/screenshot.py --port COM12 --out tienda.png [--cmd "tienda gorros"] [--wait 1.5]
  python tools/screenshot.py --port COM12 --batch capturas.txt --outdir capturas/
      (capturas.txt: una linea por captura, «nombre: comando; comando»)
"""
import argparse
import base64
import os
import sys
import time

import serial
from PIL import Image


def open_port(port, baud):
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = 0.2
    ser.dtr = False  # sin reset: EN y IO0 quedan como estan
    ser.rts = False
    ser.open()
    return ser


def send(ser, line):
    ser.write((line + "\n").encode())


def read_snap(ser, timeout=90):
    """Lee hasta «SNAP DONE»; devuelve {nombre: (w, h, fmt, bytes rle)}."""
    t0 = time.time()
    buf = b""
    cur = None
    parts = {}
    while time.time() - t0 < timeout:
        chunk = ser.read(8192)
        if not chunk:
            continue
        buf += chunk
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            text = line.decode("ascii", errors="replace").strip()
            if text.startswith("SNAP BEGIN"):
                _, _, name, w, h, fmt = text.split()
                cur = name
                parts[name] = [int(w), int(h), fmt, []]
            elif text.startswith("SNAP END"):
                cur = None
            elif text.startswith("SNAP DONE"):
                return {k: (v[0], v[1], v[2], b"".join(v[3])) for k, v in parts.items()}
            elif cur:
                try:
                    parts[cur][3].append(base64.b64decode(text))
                except Exception:
                    pass  # una linea de log colada en medio
    raise TimeoutError("sin SNAP DONE")


def decode(w, h, fmt, data):
    bpp = 2 if fmt == "rgb565" else 4
    out = bytearray()
    i = 0
    while i + 1 + bpp <= len(data):
        n = data[i]
        out += data[i + 1:i + 1 + bpp] * n
        i += 1 + bpp
    need = w * h * bpp
    out = bytes(out[:need]).ljust(need, b"\0")
    im = Image.new("RGBA", (w, h))
    px = []
    if bpp == 2:
        for k in range(0, need, 2):
            v = out[k] | (out[k + 1] << 8)
            px.append((((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31, 255))
    else:
        for k in range(0, need, 4):
            b, g, r, a = out[k:k + 4]
            px.append((r, g, b, a))
    im.putdata(px)
    return im


def capture(ser, cmds, wait):
    ser.reset_input_buffer()
    for c in cmds:
        send(ser, c)
        time.sleep(wait)
    ser.reset_input_buffer()
    send(ser, "pantalla")
    parts = read_snap(ser)
    w, h, fmt, data = parts["scr"]
    im = decode(w, h, fmt, data)
    if "top" in parts:
        im.alpha_composite(decode(*parts["top"]))
    return im


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", default="pantalla.png")
    ap.add_argument("--cmd", action="append", default=[])
    ap.add_argument("--wait", type=float, default=1.5)
    ap.add_argument("--batch")
    ap.add_argument("--outdir", default=".")
    args = ap.parse_args()
    ser = open_port(args.port, args.baud)
    try:
        if args.batch:
            os.makedirs(args.outdir, exist_ok=True)
            for line in open(args.batch, encoding="utf-8"):
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                name, _, cmds = line.partition(":")
                im = capture(ser, [c.strip() for c in cmds.split(";") if c.strip()], args.wait)
                path = os.path.join(args.outdir, name.strip() + ".png")
                im.save(path)
                print(path)
        else:
            capture(ser, args.cmd, args.wait).save(args.out)
            print(args.out)
    finally:
        ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
