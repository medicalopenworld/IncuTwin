#!/usr/bin/env python3
"""Assets adicionales (LVGL 9): marca IncuTwin (del logo), iconos de estado (termometro,
lampara), iconos de cobertura WiFi (0..3 barras + sin WiFi) y las dos imagenes a pantalla
completa (incubadora vacia, bebe con sus padres) en vertical 240x320.
Uso: python tools/gen_extra.py"""
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageOps

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
IMAGES = os.path.join(ROOT, "Images")
sys.path.insert(0, HERE)
from gen_assets import ASSETS, PREVIEW, load_logo  # noqa: E402
from lvgl9_c import c_header, emit_argb8888, emit_rgb565, write_header  # noqa: E402

NAVY = (30, 62, 110)
RED = (224, 82, 74)
GRAY = (154, 161, 170)

SCREEN_W, SCREEN_H = 240, 320  # vertical


def make_wordmark():
    logo = load_logo()
    w, h = logo.size
    # banda del texto "IncuTwin", medida sobre Images/IncuTwin_logo.png (1024x1010):
    # el icono acaba ~y=765, el texto ocupa y=796..946, el eslogan empieza ~y=964.
    band = logo.crop((0, int(h * 0.782), w, int(h * 0.941)))
    bbox = band.getbbox()
    band = band.crop(bbox)
    ratio = 26 / band.height
    return band.resize((round(band.width * ratio), 26), Image.LANCZOS)


def make_thermo():
    s = 4
    W, H = 30, 30
    im = Image.new("RGBA", (W * s, H * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    w, h = W * s, H * s
    cx = w // 2
    tw = w // 5           # semiancho del tubo
    d.rounded_rectangle((cx - tw - 3, 2, cx + tw + 3, h * 0.62), radius=tw + 3, fill=NAVY)
    d.ellipse((cx - tw * 2 - 3, h * 0.5, cx + tw * 2 + 3, h - 2), fill=NAVY)
    d.rounded_rectangle((cx - tw, 6, cx + tw, h * 0.62), radius=tw, fill=(245, 245, 242))
    d.ellipse((cx - tw * 2, h * 0.5 + 3, cx + tw * 2, h - 5), fill=RED)
    d.rectangle((cx - tw + 2, h * 0.3, cx + tw - 2, h * 0.62), fill=RED)
    return im.resize((W, H), Image.LANCZOS)


def make_lamp():
    s = 4
    W, H = 30, 30
    im = Image.new("RGBA", (W * s, H * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    w, h = W * s, H * s
    cx = w // 2
    blue = (95, 156, 222)
    d.rectangle((cx - 3, 0, cx + 3, h * 0.12), fill=NAVY)
    d.polygon([(cx - w * 0.16, h * 0.10), (cx + w * 0.16, h * 0.10),
               (cx + w * 0.34, h * 0.42), (cx - w * 0.34, h * 0.42)], fill=NAVY)
    d.polygon([(cx - w * 0.11, h * 0.16), (cx + w * 0.11, h * 0.16),
               (cx + w * 0.26, h * 0.38), (cx - w * 0.26, h * 0.38)], fill=(250, 215, 120))
    for dx in (-0.26, 0.0, 0.26):
        x0 = cx + dx * w
        d.line((x0, h * 0.5, x0 + dx * w * 0.4, h * 0.92), fill=blue, width=6)
    return im.resize((W, H), Image.LANCZOS)


def make_wifi(level):
    """Abanico WiFi: punto + 3 arcos. `level` 0..3 = arcos solidos; el resto tenue (mismo
    navy con poca alfa) para que un recolor de LVGL conserve la distincion."""
    s = 8
    W, H = 24, 24
    im = Image.new("RGBA", (W * s, H * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx, cy = W * s // 2, (H - 3) * s
    d.ellipse((cx - 2 * s, cy - 2 * s, cx + 2 * s, cy + 2 * s), fill=NAVY + (255,))
    for i, r in enumerate((7, 12, 17)):
        a = 255 if level > i else 55
        d.arc((cx - r * s, cy - r * s, cx + r * s, cy + r * s), 225, 315,
              fill=NAVY + (a,), width=3 * s)
    return im.resize((W, H), Image.LANCZOS)


def make_wifi_off():
    """Abanico gris con barra roja: el panel no esta en ninguna WiFi."""
    s = 8
    W, H = 24, 24
    im = Image.new("RGBA", (W * s, H * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx, cy = W * s // 2, (H - 3) * s
    d.ellipse((cx - 2 * s, cy - 2 * s, cx + 2 * s, cy + 2 * s), fill=GRAY + (255,))
    for r in (7, 12, 17):
        d.arc((cx - r * s, cy - r * s, cx + r * s, cy + r * s), 225, 315,
              fill=GRAY + (140,), width=3 * s)
    d.line((4 * s, 3 * s, (W - 4) * s, (H - 2) * s), fill=RED + (255,), width=3 * s)
    return im.resize((W, H), Image.LANCZOS)


def fullscreen_from(png):
    """Imagen derecha en la pantalla vertical: recorta margenes blancos y rellena a 240x320."""
    im = Image.open(os.path.join(IMAGES, png)).convert("RGB")
    bg = Image.new("RGB", im.size, (255, 255, 255))
    diff = ImageChops.difference(im, bg).convert("L")
    bbox = diff.point(lambda v: 255 if v > 16 else 0).getbbox()
    im = im.crop(bbox)
    return ImageOps.pad(im, (SCREEN_W, SCREEN_H), Image.LANCZOS, color=(255, 255, 255))


def main():
    os.makedirs(PREVIEW, exist_ok=True)
    os.makedirs(os.path.join(ASSETS, "include"), exist_ok=True)
    wm = make_wordmark()
    th = make_thermo()
    lp = make_lamp()
    wifis = [make_wifi(i) for i in range(4)]
    wifi_off = make_wifi_off()
    empty = fullscreen_from("IncuNest_empty.png")
    parents = fullscreen_from("Baby_with_parents.png")

    wm.save(os.path.join(PREVIEW, "wordmark.png"))
    th.save(os.path.join(PREVIEW, "icon_thermo.png"))
    lp.save(os.path.join(PREVIEW, "icon_lamp.png"))
    wifi_off.save(os.path.join(PREVIEW, "wifi_off.png"))
    for i, w in enumerate(wifis):
        w.save(os.path.join(PREVIEW, f"wifi_{i}.png"))
    empty.save(os.path.join(PREVIEW, "incunest_empty.png"))
    parents.save(os.path.join(PREVIEW, "baby_parents.png"))
    print("wordmark:", wm.size, " empty:", empty.size)

    with open(os.path.join(ASSETS, "img_extra.c"), "w", encoding="utf-8", newline="\n") as f:
        f.write(c_header("Marca, iconos de estado, cobertura WiFi e imagenes a pantalla completa"))
        emit_argb8888(f, "img_wordmark", wm)
        emit_argb8888(f, "img_icon_thermo", th)
        emit_argb8888(f, "img_icon_lamp", lp)
        emit_argb8888(f, "img_wifi_off", wifi_off)
        for i, w in enumerate(wifis):
            emit_argb8888(f, f"img_wifi_{i}", w)
        emit_rgb565(f, "img_incunest_empty", empty)
        emit_rgb565(f, "img_baby_parents", parents)

    write_header(
        os.path.join(ASSETS, "include", "assets_extra.h"),
        "Assets adicionales (gen_extra.py)",
        ["img_wordmark", "img_icon_thermo", "img_icon_lamp", "img_wifi_off",
         "img_wifi_0", "img_wifi_1", "img_wifi_2", "img_wifi_3",
         "img_incunest_empty", "img_baby_parents"],
    )
    print("img_extra.c escrito")


if __name__ == "__main__":
    main()
