#!/usr/bin/env python3
"""Regenera las fuentes LVGL 9 de IncuTwin (Montserrat Medium + simbolos FontAwesome 5).

Requiere `npm i -g lv_font_conv` y las fuentes origen (ver --src):
  Montserrat-Medium.ttf  https://github.com/JulietaUla/Montserrat (fonts/ttf/)
  fa-solid-900.woff      https://github.com/FortAwesome/Font-Awesome/tree/5.15.4/webfonts

Uso: python tools/gen_fonts.py --src <dir con las dos fuentes>
Salida: components/assets/fonts/lv_font_es_{12,14,16,20}.c
"""
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "components", "assets", "fonts")

SIZES = (12, 14, 16, 20)
LATIN_RANGE = "0x20-0x7F,0xB0,0x2022,0xB7"   # ASCII + grado + bullet + punto medio
ES_SYMBOLS = "ÁÉÍÓÚÑÜáéíóúñü¡¿"
# Simbolos LV_SYMBOL_* de LVGL presentes en FontAwesome 5 Solid. Faltan a proposito los
# de "Brands" (USB 0xF287, Bluetooth 0xF293) y NEW_LINE 0xF8A2 (no existe en FA 5.15).
FA_CODEPOINTS = (
    "61441,61448,61451,61452,61453,61457,61459,61461,61465,61468,61473,61478,61479,61480,"
    "61502,61507,61512,61515,61516,61517,61521,61522,61523,61524,61543,61544,61550,61552,"
    "61553,61556,61559,61560,61561,61563,61587,61589,61636,61637,61639,61641,61664,61671,"
    "61674,61683,61724,61732,61787,61931,62016,62017,62018,62019,62020,62212,62189,62810,63426"
)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="directorio con Montserrat-Medium.ttf y fa-solid-900.woff")
    args = ap.parse_args()
    conv = shutil.which("lv_font_conv") or shutil.which("lv_font_conv.cmd")
    if not conv:
        sys.exit("lv_font_conv no encontrado: npm i -g lv_font_conv")
    os.makedirs(OUT, exist_ok=True)
    ttf = os.path.join(args.src, "Montserrat-Medium.ttf")
    woff = os.path.join(args.src, "fa-solid-900.woff")
    for sz in SIZES:
        out = os.path.join(OUT, f"lv_font_es_{sz}.c")
        cmd = [conv, "--no-compress", "--no-prefilter", "--bpp", "4", "--size", str(sz),
               "--lv-include", "lvgl.h", "--format", "lvgl", "--force-fast-kern-format",
               "--font", ttf, "-r", LATIN_RANGE, "--symbols", ES_SYMBOLS,
               "--font", woff, "-r", FA_CODEPOINTS, "-o", out]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(out):
            sys.exit(f"fallo en {sz} px:\n{r.stdout}\n{r.stderr}")
        print(f"ok lv_font_es_{sz}.c ({os.path.getsize(out) // 1024} KB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
