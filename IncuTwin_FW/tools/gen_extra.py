#!/usr/bin/env python3
"""Extra assets: IncuTwin wordmark (from logo), status icons (thermo, lamp),
WiFi coverage icons (0..3 bars + off) and the full-screen "empty incubator"
image for the offline view (portrait 240x320, content rotated 90 deg).
Usage: python3 tools/gen_extra.py"""
import os
import sys
from PIL import Image, ImageChops, ImageDraw, ImageOps

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
IMAGES = os.path.join(ROOT, "Images")
sys.path.insert(0, HERE)
from gen_assets import c_header, emit_true_color_alpha, load_logo, PREVIEW, ASSETS

NAVY = (30, 62, 110)
RED = (224, 82, 74)
GRAY = (154, 161, 170)

SCREEN_W, SCREEN_H = 240, 320  # portrait


def make_wordmark():
    logo = load_logo()
    w, h = logo.size
    # text band: between hands and tagline. Measured on the actual asset
    # (Images/IncuTwin_logo.png, 1024x1010): icon ends ~y=765, "IncuTwin"
    # text spans y=796..946, tagline starts ~y=964. The old 0.735/0.925
    # fractions clipped the bottom of the letters and caught icon
    # fingertip fragments at the top.
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
    tw = w // 5           # tube half-width
    # outline
    d.rounded_rectangle((cx - tw - 3, 2, cx + tw + 3, h * 0.62), radius=tw + 3,
                        fill=NAVY)
    d.ellipse((cx - tw * 2 - 3, h * 0.5, cx + tw * 2 + 3, h - 2), fill=NAVY)
    # tube + bulb
    d.rounded_rectangle((cx - tw, 6, cx + tw, h * 0.62), radius=tw,
                        fill=(245, 245, 242))
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
    # lamp head (trapezoid) + hanging stem
    d.rectangle((cx - 3, 0, cx + 3, h * 0.12), fill=NAVY)
    d.polygon([(cx - w * 0.16, h * 0.10), (cx + w * 0.16, h * 0.10),
               (cx + w * 0.34, h * 0.42), (cx - w * 0.34, h * 0.42)], fill=NAVY)
    d.polygon([(cx - w * 0.11, h * 0.16), (cx + w * 0.11, h * 0.16),
               (cx + w * 0.26, h * 0.38), (cx - w * 0.26, h * 0.38)],
              fill=(250, 215, 120))
    # light rays
    for dx in (-0.26, 0.0, 0.26):
        x0 = cx + dx * w
        d.line((x0, h * 0.5, x0 + dx * w * 0.4, h * 0.92), fill=blue, width=6)
    return im.resize((W, H), Image.LANCZOS)


def make_wifi(level):
    """WiFi fan: dot + 3 arcs. `level` 0..3 = arcs drawn solid; the rest stay
    faint (same navy, low alpha) so an LVGL recolor keeps the distinction."""
    s = 8
    W, H = 24, 24
    im = Image.new("RGBA", (W * s, H * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx, cy = W * s // 2, (H - 3) * s
    d.ellipse((cx - 2 * s, cy - 2 * s, cx + 2 * s, cy + 2 * s),
              fill=NAVY + (255,))
    for i, r in enumerate((7, 12, 17)):
        a = 255 if level > i else 55
        d.arc((cx - r * s, cy - r * s, cx + r * s, cy + r * s),
              225, 315, fill=NAVY + (a,), width=3 * s)
    return im.resize((W, H), Image.LANCZOS)


def make_wifi_off():
    """Gray fan with a red slash: panel not joined to any WiFi."""
    s = 8
    W, H = 24, 24
    im = Image.new("RGBA", (W * s, H * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx, cy = W * s // 2, (H - 3) * s
    d.ellipse((cx - 2 * s, cy - 2 * s, cx + 2 * s, cy + 2 * s),
              fill=GRAY + (255,))
    for r in (7, 12, 17):
        d.arc((cx - r * s, cy - r * s, cx + r * s, cy + r * s),
              225, 315, fill=GRAY + (140,), width=3 * s)
    d.line((4 * s, 3 * s, (W - 4) * s, (H - 2) * s), fill=RED + (255,),
           width=3 * s)
    return im.resize((W, H), Image.LANCZOS)


def make_incunest_empty():
    """Images/IncuNest_empty.png upright on the portrait screen: the
    incubator reads naturally, with white bands above and below."""
    im = Image.open(os.path.join(IMAGES, "IncuNest_empty.png")).convert("RGB")
    # crop the white margins around the drawing
    bg = Image.new("RGB", im.size, (255, 255, 255))
    diff = ImageChops.difference(im, bg).convert("L")
    bbox = diff.point(lambda v: 255 if v > 16 else 0).getbbox()
    im = im.crop(bbox)
    return ImageOps.pad(im, (SCREEN_W, SCREEN_H), Image.LANCZOS,
                        color=(255, 255, 255))


def make_baby_parents():
    """Images/Baby_with_parents.png upright on the portrait screen, same
    treatment as the empty incubator: crop white margins, pad to 240x320."""
    im = Image.open(os.path.join(IMAGES, "Baby_with_parents.png")).convert("RGB")
    bg = Image.new("RGB", im.size, (255, 255, 255))
    diff = ImageChops.difference(im, bg).convert("L")
    bbox = diff.point(lambda v: 255 if v > 16 else 0).getbbox()
    im = im.crop(bbox)
    return ImageOps.pad(im, (SCREEN_W, SCREEN_H), Image.LANCZOS,
                        color=(255, 255, 255))


def emit_true_color(name, im, f):
    """RGB565 without alpha (full-screen backgrounds: 2 bytes/px)."""
    w, h = im.size
    px = im.convert("RGB").load()
    f.write(f"static const uint8_t {name}_map[] = {{\n")
    for y in range(h):
        vals = []
        for x in range(w):
            r, g, b = px[x, y]
            c565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            vals += [c565 & 0xFF, c565 >> 8]
        f.write("  " + ", ".join(f"0x{v:02x}" for v in vals) + ",\n")
    f.write("};\n\n")
    f.write(f"const lv_img_dsc_t {name} = {{\n")
    f.write("  .header.cf = LV_IMG_CF_TRUE_COLOR,\n")
    f.write("  .header.always_zero = 0,\n")
    f.write("  .header.reserved = 0,\n")
    f.write(f"  .header.w = {w},\n  .header.h = {h},\n")
    f.write(f"  .data_size = {w * h * 2},\n")
    f.write(f"  .data = {name}_map,\n}};\n\n")


EXTERNS = """extern const lv_img_dsc_t img_wordmark;
extern const lv_img_dsc_t img_icon_thermo;
extern const lv_img_dsc_t img_icon_lamp;
extern const lv_img_dsc_t img_wifi_off;
extern const lv_img_dsc_t img_wifi_0;
extern const lv_img_dsc_t img_wifi_1;
extern const lv_img_dsc_t img_wifi_2;
extern const lv_img_dsc_t img_wifi_3;
extern const lv_img_dsc_t img_incunest_empty;
extern const lv_img_dsc_t img_baby_parents;
"""


def main():
    os.makedirs(PREVIEW, exist_ok=True)
    os.makedirs(ASSETS, exist_ok=True)
    wm = make_wordmark()
    th = make_thermo()
    lp = make_lamp()
    wifis = [make_wifi(i) for i in range(4)]
    wifi_off = make_wifi_off()
    empty = make_incunest_empty()
    parents = make_baby_parents()

    wm.save(os.path.join(PREVIEW, "wordmark.png"))
    th.save(os.path.join(PREVIEW, "icon_thermo.png"))
    lp.save(os.path.join(PREVIEW, "icon_lamp.png"))
    wifi_off.save(os.path.join(PREVIEW, "wifi_off.png"))
    for i, w in enumerate(wifis):
        w.save(os.path.join(PREVIEW, f"wifi_{i}.png"))
    empty.save(os.path.join(PREVIEW, "incunest_empty.png"))
    parents.save(os.path.join(PREVIEW, "baby_parents.png"))
    print("wordmark:", wm.size, " empty:", empty.size)

    with open(os.path.join(ASSETS, "img_extra.c"), "w") as f:
        f.write(c_header())
        emit_true_color_alpha("img_wordmark", wm, f)
        emit_true_color_alpha("img_icon_thermo", th, f)
        emit_true_color_alpha("img_icon_lamp", lp, f)
        emit_true_color_alpha("img_wifi_off", wifi_off, f)
        for i, w in enumerate(wifis):
            emit_true_color_alpha(f"img_wifi_{i}", w, f)
        emit_true_color("img_incunest_empty", empty, f)
        emit_true_color("img_baby_parents", parents, f)

    # append externs to assets.h if missing (gen_assets.py rewrites the file)
    hpath = os.path.join(ASSETS, "assets.h")
    src = open(hpath).read()
    if "img_incunest_empty" not in src:
        # drop any older partial block first
        for line in EXTERNS.splitlines():
            src = src.replace(line + "\n", "")
        src = src.replace("extern const lv_img_dsc_t img_logo;\n",
                          "extern const lv_img_dsc_t img_logo;\n" + EXTERNS)
        open(hpath, "w").write(src)
    print("img_extra.c written")


if __name__ == "__main__":
    main()
