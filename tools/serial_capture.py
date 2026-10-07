#!/usr/bin/env python3
"""Captura el log serie de una IncuTwin durante N segundos y lo imprime.

Sustituye a `idf.py monitor` (interactivo) en las verificaciones automatizadas:
abre el puerto, opcionalmente resetea la placa por DTR/RTS, y vuelca lo recibido.

Uso:
  python tools/serial_capture.py --port COM12 --seconds 10 [--reset] [--grep "incutwin"]
         [--send "info"]  (envia una linea por la consola serie tras 2 s)

Requiere pyserial (viene con el entorno Python de ESP-IDF).
"""
import argparse
import sys
import time

import serial


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=10.0)
    ap.add_argument("--reset", action="store_true", help="resetear la placa al abrir")
    ap.add_argument("--grep", help="solo lineas que contengan este texto")
    ap.add_argument("--send", help="linea a enviar por la consola tras 2 s")
    args = ap.parse_args()

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        if args.reset:
            # Secuencia clasica del auto-reset de los puentes USB-serie del ESP32:
            # EN bajo con RTS, IO0 alto (no entrar en descarga), soltar.
            ser.dtr = False
            ser.rts = True
            time.sleep(0.1)
            ser.rts = False
        t0 = time.time()
        sent = False
        buf = b""
        while time.time() - t0 < args.seconds:
            if args.send and not sent and time.time() - t0 > 2.0:
                ser.write((args.send + "\n").encode())
                sent = True
            chunk = ser.read(4096)
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("utf-8", errors="replace").rstrip("\r")
                if args.grep and args.grep not in text:
                    continue
                print(text)
                sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
