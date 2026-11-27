#!/usr/bin/env python3
"""Regenera las fuentes LVGL 9 de IncuTwin con la letra de la app (estilo «IncuTwin de bolsillo»).

- Texto (lv_font_es_12/14/16/20): Nunito Bold + simbolos FontAwesome 5 (LV_SYMBOL_*).
- Titulos, botones y numeros (lv_font_title_12/14/16/20/24): Fredoka SemiBold + los mismos simbolos.

Requiere `npm i -g lv_font_conv` y las fuentes origen (OFL / licencia de FontAwesome):
  Nunito_700Bold.ttf, Fredoka_600SemiBold.ttf  (Google Fonts; tambien en @expo-google-fonts/*)
  FontAwesome5-Solid+Brands+Regular.woff         (lvgl/scripts/built_in_font/)

Uso: python tools/gen_fonts.py --body <Nunito.ttf> --title <Fredoka.ttf> --fa <FontAwesome.woff>
Salida: components/assets/fonts/lv_font_es_{12,14,16,20}.c y lv_font_title_{12,14,16,20,24}.c
"""
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "components", "assets", "fonts")

FAMILIES = (("lv_font_es", "body", (12, 14, 16, 20)), ("lv_font_title", "title", (12, 14, 16, 20, 24)))
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
    ap.add_argument("--body", required=True, help="Nunito_700Bold.ttf")
    ap.add_argument("--title", required=True, help="Fredoka_600SemiBold.ttf")
    ap.add_argument("--fa", required=True, help="FontAwesome5-Solid+Brands+Regular.woff (o fa-solid-900.woff)")
    args = ap.parse_args()
    conv = shutil.which("lv_font_conv") or shutil.which("lv_font_conv.cmd")
    if not conv:
        sys.exit("lv_font_conv no encontrado: npm i -g lv_font_conv")
    conv = [conv]
    # En Windows el lanzador .cmd de npm se lia con ¡¿ en los argumentos: node + el script
    js = os.path.join(os.path.dirname(conv[0]), "node_modules", "lv_font_conv", "lv_font_conv.js")
    if os.name == "nt" and os.path.exists(js) and shutil.which("node"):
        conv = [shutil.which("node"), js]
    os.makedirs(OUT, exist_ok=True)
    src = {"body": args.body, "title": args.title}
    for name, role, sizes in FAMILIES:
        for sz in sizes:
            out = os.path.join(OUT, f"{name}_{sz}.c")
            cmd = conv + ["--no-compress", "--no-prefilter", "--bpp", "4", "--size", str(sz),
                   "--lv-include", "lvgl.h", "--format", "lvgl", "--force-fast-kern-format",
                   "--font", src[role], "-r", LATIN_RANGE, "--symbols", ES_SYMBOLS,
                   "--font", args.fa, "-r", FA_CODEPOINTS, "-o", out]
            r = subprocess.run(cmd, capture_output=True, text=True)
            if r.returncode != 0 or not os.path.exists(out):
                sys.exit(f"fallo en {name} {sz} px:\n{r.stdout}\n{r.stderr}")
            # sin rutas locales en la cabecera del fichero generado
            with open(out, encoding="utf-8") as f:
                txt = f.read()
            for p in (src[role], args.fa, out):
                txt = txt.replace(p, os.path.basename(p))
            with open(out, "w", encoding="utf-8", newline="\n") as f:
                f.write(txt)
            print(f"ok {name}_{sz}.c ({os.path.getsize(out) // 1024} KB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
