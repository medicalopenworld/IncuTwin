#!/usr/bin/env python3
"""Banderas de los paises de twin_geo.c para la pagina del pais (spec twin-display).

Origen: tools/data/flags/<iso>.png (flagcdn.com, 80 px de ancho; las banderas nacionales son de
dominio publico). Salida: 26 px de alto, ancho segun la proporcion de cada bandera (max. 52),
esquinas redondeadas y borde gris fino, ARGB8888, mas flag_image(iso) para buscarlas.
Uso: python tools/gen_flags.py"""
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gen_assets import ASSETS, PREVIEW  # noqa: E402
from lvgl9_c import c_header, emit_argb8888, write_header  # noqa: E402

SRC = os.path.join(HERE, "data", "flags")
H = 26
MAX_W = 52
S = 4


def make_flag(path):
    src = Image.open(path).convert("RGBA")
    w = min(MAX_W, round(src.width * H / src.height))
    big = src.resize((w * S, H * S), Image.LANCZOS)
    mask = Image.new("L", big.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, w * S - 1, H * S - 1), radius=3 * S, fill=255)
    out = Image.new("RGBA", big.size, (0, 0, 0, 0))
    out.paste(big, mask=mask)
    ImageDraw.Draw(out).rounded_rectangle((0, 0, w * S - 1, H * S - 1), radius=3 * S,
                                          outline=(190, 196, 204, 255), width=S)
    return out.resize((w, H), Image.LANCZOS)


def main():
    isos = sorted(f[:-4].upper() for f in os.listdir(SRC) if f.endswith(".png"))
    names = [f"img_cflag_{iso.lower()}" for iso in isos]  # img_flag_cm ya existe (gen_extra)
    prev = os.path.join(PREVIEW, "flags")
    os.makedirs(prev, exist_ok=True)
    with open(os.path.join(ASSETS, "img_flags.c"), "w", encoding="utf-8", newline="\n") as f:
        f.write(c_header("Banderas de la pagina del pais (flagcdn.com, dominio publico)"))
        f.write('#include <stddef.h>\n#include "assets_flags.h"\n\n')
        for iso, name in zip(isos, names):
            im = make_flag(os.path.join(SRC, iso.lower() + ".png"))
            im.save(os.path.join(prev, iso.lower() + ".png"))
            emit_argb8888(f, name, im)
        f.write("const lv_image_dsc_t *flag_image(const char *iso)\n{\n")
        f.write("    static const struct { char iso[3]; const lv_image_dsc_t *img; } T[] = {\n")
        for iso, name in zip(isos, names):
            f.write(f'        {{ "{iso}", &{name} }},\n')
        f.write("    };\n    if (!iso || !iso[0]) return NULL;\n")
        f.write("    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++) {\n")
        f.write("        if (T[i].iso[0] == iso[0] && T[i].iso[1] == iso[1]) return T[i].img;\n")
        f.write("    }\n    return NULL;\n}\n")
    write_header(os.path.join(ASSETS, "include", "assets_flags.h"), "Banderas (gen_flags.py)", [],
                 "/* Bandera del pais ISO alfa-2 (26 px de alto) o NULL si no hay. */\n"
                 "const lv_image_dsc_t *flag_image(const char *iso);\n")
    print(f"img_flags.c escrito: {len(isos)} banderas")


if __name__ == "__main__":
    main()
