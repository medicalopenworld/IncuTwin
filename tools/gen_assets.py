#!/usr/bin/env python3
"""
IncuTwin asset generator for the CrowPanel Advance 2.8 (LVGL 8).

Generates, from Images/Baby.png and Images/IncuTwin_logo.png:
  - Baby animation frames (sleep base, yawn x2, awake) as LVGL
    INDEXED-8BIT C arrays sharing one palette.
  - 6 skin-tone palettes (only the skin entries differ).
  - Heart sprite (true color alpha).
  - Logo splash (true color alpha).

Usage:  python3 tools/gen_assets.py [--preview-only]
Previews go to tools/preview/, C files to src/assets/.
"""
import os
import sys
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
IMAGES = os.path.join(ROOT, "Images")
PREVIEW = os.path.join(HERE, "preview")
ASSETS = os.path.join(ROOT, "src", "assets")

BABY_H = 185          # final baby sprite height in px
SKIN_BASE = (244, 193, 166)   # original skin colour of Baby.png
BLUSH_BASE = (239, 148, 146)  # approximate blush colour
NAVY = (21, 38, 82)
MOUTH_FILL = (156, 88, 96)
TONGUE = (232, 150, 150)

# 6 configurable skin tones (skin, blush)
SKIN_TONES = [
    ((244, 193, 166), (239, 148, 146)),  # 0 light (original)
    ((233, 175, 140), (226, 136, 130)),  # 1 medium light
    ((208, 148, 108), (198, 116, 104)),  # 2 medium
    ((172, 112, 78),  (168, 92, 84)),    # 3 tan
    ((134, 84, 56),   (140, 74, 68)),    # 4 brown
    ((92, 58, 40),    (104, 56, 52)),    # 5 dark
]


def is_skin(p):
    r, g, b = p[:3]
    # skin is warm AND clearly r>g (pale-yellow moons on the blanket have r~g)
    return (r > 150 and r > b + 35 and r - g > 30 and g > b
            and abs(g - 193) < 80 and b < 210)


def is_blush(p):
    r, g, b = p[:3]
    return r > 200 and g < 185 and b < 185 and r - g > 55 and abs(g - b) < 30


def load_baby():
    im = Image.open(os.path.join(IMAGES, "Baby.png")).convert("RGBA")
    w, h = im.size
    px = im.load()

    def near_white(p, tol=24):
        return p[0] > 255 - tol and p[1] > 255 - tol and p[2] > 255 - tol

    from collections import deque
    seen = [[False] * w for _ in range(h)]
    dq = deque()
    for x in range(w):
        dq.append((x, 0)); dq.append((x, h - 1))
    for y in range(h):
        dq.append((0, y)); dq.append((w - 1, y))
    while dq:
        x, y = dq.popleft()
        if x < 0 or y < 0 or x >= w or y >= h or seen[y][x]:
            continue
        seen[y][x] = True
        if not near_white(px[x, y]):
            continue
        px[x, y] = (255, 255, 255, 0)
        dq.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))

    bbox = im.getbbox()
    im = im.crop(bbox)
    ratio = BABY_H / im.height
    im = im.resize((round(im.width * ratio), BABY_H), Image.LANCZOS)
    return im


def face_regions(im):
    """Return face bbox based on skin pixels."""
    w, h = im.size
    px = im.load()
    xs, ys = [], []
    for y in range(h):
        for x in range(w):
            p = px[x, y]
            if p[3] > 128 and is_skin(p):
                xs.append(x); ys.append(y)
    return min(xs), max(xs), min(ys), max(ys)


def make_frames(base):
    """Return dict name->RGBA frame."""
    fx0, fx1, fy0, fy1 = face_regions(base)
    fw = fx1 - fx0
    fh = fy1 - fy0
    cx = (fx0 + fx1) // 2
    mouth_y = fy0 + int(fh * 0.72)

    frames = {"sleep": base.copy()}

    # --- yawn frames: cover smile, draw open mouth -------------------------
    for name, rw, rh in (("yawn1", 0.11, 0.08), ("yawn2", 0.17, 0.15)):
        f = base.copy()
        d = ImageDraw.Draw(f)
        clear = (cx - int(fw * 0.18), mouth_y - int(fh * 0.14),
                 cx + int(fw * 0.18), mouth_y + int(fh * 0.16))
        d.rectangle(clear, fill=SKIN_BASE + (255,))
        rx, ry = int(fw * rw), int(fh * rh)
        d.ellipse((cx - rx - 2, mouth_y - ry - 2, cx + rx + 2, mouth_y + ry + 2),
                  fill=NAVY + (255,))
        d.ellipse((cx - rx, mouth_y - ry, cx + rx, mouth_y + ry),
                  fill=MOUTH_FILL + (255,))
        if ry >= 5:
            d.ellipse((cx - rx // 2, mouth_y + ry // 4,
                       cx + rx // 2, mouth_y + ry - 1), fill=TONGUE + (255,))
        frames[name] = f

    # --- awake frame: cover closed-eye arcs, draw round eyes ---------------
    f = base.copy()
    d = ImageDraw.Draw(f)
    eye_y = fy0 + int(fh * 0.40)
    eye_dx = int(fw * 0.24)
    er = max(3, int(fw * 0.06))
    for ex in (cx - eye_dx, cx + eye_dx):
        d.rectangle((ex - er * 2, eye_y - er * 2, ex + er * 2, eye_y + er),
                    fill=SKIN_BASE + (255,))
        d.ellipse((ex - er, eye_y - er, ex + er, eye_y + er), fill=NAVY + (255,))
        d.ellipse((ex - er // 3, eye_y - er + 1, ex + er // 4, eye_y - er // 3),
                  fill=(235, 240, 250, 255))
    frames["awake"] = f
    return frames


def make_heart():
    s = 4
    W, H = 34, 30
    im = Image.new("RGBA", (W * s, H * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    w, h = W * s, H * s
    r = w // 4
    red = (224, 82, 74)
    d.ellipse((0, 0, 2 * r, 2 * r), fill=red)
    d.ellipse((w - 2 * r - 1, 0, w - 1, 2 * r), fill=red)
    d.polygon([(1, r + r // 2), (w - 2, r + r // 2), (w // 2, h - 1)], fill=red)
    d.polygon([(2, r), (w - 2, r), (w // 2, h - 6)], fill=red)
    d.ellipse((r * 0.45, r * 0.4, r * 1.05, r * 1.0), fill=(250, 200, 195, 255))
    im = im.resize((W, H), Image.LANCZOS)
    return im


def quantize_frames(frames, ncolors=48):
    """Joint quantization of all frames so they share a palette."""
    names = list(frames.keys())
    w, h = frames[names[0]].size
    strip = Image.new("RGBA", (w * len(names), h), (0, 0, 0, 0))
    for i, n in enumerate(names):
        strip.paste(frames[n], (w * i, 0))
    rgb = strip.convert("RGB")
    q = rgb.quantize(colors=ncolors - 1, method=Image.MEDIANCUT)
    pal = q.getpalette()[: (ncolors - 1) * 3]
    palette = [(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2], 255)
               for i in range(ncolors - 1)]
    palette.insert(0, (0, 0, 0, 0))  # index 0 = transparent
    qpx = q.load()
    spx = strip.load()
    out = {}
    for i, n in enumerate(names):
        idxs = []
        for y in range(h):
            for x in range(w):
                if spx[w * i + x, y][3] < 128:
                    idxs.append(0)
                else:
                    idxs.append(qpx[w * i + x, y] + 1)
        out[n] = idxs
    return palette, out, (w, h)


def skin_indices(palette):
    res = []
    for i, (r, g, b, a) in enumerate(palette):
        if a == 0:
            continue
        if is_blush((r, g, b)):
            res.append((i, "blush"))
        elif is_skin((r, g, b)):
            res.append((i, "skin"))
    return res


def tone_color(orig, base_from, base_to):
    out = []
    for c, bf, bt in zip(orig, base_from, base_to):
        v = c * (bt / max(bf, 1))
        out.append(max(0, min(255, round(v))))
    return tuple(out)


def build_tone_palettes(palette, skin_idx):
    pals = []
    for skin_to, blush_to in SKIN_TONES:
        p = list(palette)
        for i, kind in skin_idx:
            r, g, b, a = palette[i]
            if kind == "skin":
                nc = tone_color((r, g, b), SKIN_BASE, skin_to)
            else:
                nc = tone_color((r, g, b), BLUSH_BASE, blush_to)
            p[i] = nc + (255,)
        pals.append(p)
    return pals


# ---------------------------------------------------------------- C output
def c_header():
    return ('#ifdef __has_include\n'
            '#if __has_include("lvgl.h")\n'
            '#ifndef LV_LVGL_H_INCLUDE_SIMPLE\n'
            '#define LV_LVGL_H_INCLUDE_SIMPLE\n#endif\n#endif\n#endif\n\n'
            '#if defined(LV_LVGL_H_INCLUDE_SIMPLE)\n#include "lvgl.h"\n'
            '#else\n#include "lvgl/lvgl.h"\n#endif\n\n')


def emit_indexed(name, palette, idxs, size, f):
    w, h = size
    f.write(f"static const uint8_t {name}_map[] = {{\n")
    f.write("  /* palette (256 x BGRA) */\n")
    for i in range(256):
        r, g, b, a = palette[i] if i < len(palette) else (0, 0, 0, 0)
        f.write(f"  0x{b:02x}, 0x{g:02x}, 0x{r:02x}, 0x{a:02x},\n")
    f.write("  /* pixel indices */\n")
    for y in range(h):
        row = idxs[y * w:(y + 1) * w]
        f.write("  " + ", ".join(f"0x{v:02x}" for v in row) + ",\n")
    f.write("};\n\n")
    f.write(f"const lv_img_dsc_t {name} = {{\n")
    f.write("  .header.cf = LV_IMG_CF_INDEXED_8BIT,\n")
    f.write("  .header.always_zero = 0,\n")
    f.write("  .header.reserved = 0,\n")
    f.write(f"  .header.w = {w},\n  .header.h = {h},\n")
    f.write(f"  .data_size = {256 * 4 + w * h},\n")
    f.write(f"  .data = {name}_map,\n}};\n\n")


def emit_true_color_alpha(name, im, f):
    w, h = im.size
    px = im.load()
    f.write(f"static const uint8_t {name}_map[] = {{\n")
    for y in range(h):
        vals = []
        for x in range(w):
            r, g, b, a = px[x, y]
            c565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            vals += [c565 & 0xFF, c565 >> 8, a]
        f.write("  " + ", ".join(f"0x{v:02x}" for v in vals) + ",\n")
    f.write("};\n\n")
    f.write(f"const lv_img_dsc_t {name} = {{\n")
    f.write("  .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n")
    f.write("  .header.always_zero = 0,\n")
    f.write("  .header.reserved = 0,\n")
    f.write(f"  .header.w = {w},\n  .header.h = {h},\n")
    f.write(f"  .data_size = {w * h * 3},\n")
    f.write(f"  .data = {name}_map,\n}};\n\n")


def emit_palettes(pals, f):
    f.write(f"const uint8_t baby_skin_palettes[{len(pals)}][256 * 4] = {{\n")
    for p in pals:
        f.write("  {\n")
        for i in range(256):
            r, g, b, a = p[i] if i < len(p) else (0, 0, 0, 0)
            f.write(f"    0x{b:02x}, 0x{g:02x}, 0x{r:02x}, 0x{a:02x},\n")
        f.write("  },\n")
    f.write("};\n\n")


def load_logo():
    im = Image.open(os.path.join(IMAGES, "IncuTwin_logo.png")).convert("RGBA")
    px = im.load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if r > 228 and g > 228 and b > 224:
                px[x, y] = (r, g, b, 0)
    return im


def main():
    preview_only = "--preview-only" in sys.argv
    os.makedirs(PREVIEW, exist_ok=True)
    os.makedirs(ASSETS, exist_ok=True)

    base = load_baby()
    frames = make_frames(base)
    heart = make_heart()

    palette, idxmaps, size = quantize_frames(frames)
    sidx = skin_indices(palette)
    pals = build_tone_palettes(palette, sidx)

    # previews ---------------------------------------------------------------
    w, h = size
    names = list(frames.keys())
    sheet = Image.new("RGBA", (w * len(names), h), (250, 250, 248, 255))
    for i, n in enumerate(names):
        img = Image.new("RGBA", (w, h))
        p = img.load()
        for y in range(h):
            for x in range(w):
                p[x, y] = palette[idxmaps[n][y * w + x]]
        sheet.paste(img, (w * i, 0), img)
    sheet.save(os.path.join(PREVIEW, "frames.png"))

    tones = Image.new("RGBA", (w * len(pals), h), (250, 250, 248, 255))
    for t, pal in enumerate(pals):
        img = Image.new("RGBA", (w, h))
        p = img.load()
        for y in range(h):
            for x in range(w):
                p[x, y] = tuple(pal[idxmaps["sleep"][y * w + x]])
        tones.paste(img, (w * t, 0), img)
    tones.save(os.path.join(PREVIEW, "tones.png"))
    heart.save(os.path.join(PREVIEW, "heart.png"))
    print(f"frames: {names}  size={size}  palette={len(palette)} "
          f"skin_entries={len(sidx)}")
    if preview_only:
        return

    # C output -----------------------------------------------------------------
    with open(os.path.join(ASSETS, "img_baby.c"), "w") as f:
        f.write(c_header())
        for n in names:
            emit_indexed(f"img_baby_{n}", palette, idxmaps[n], size, f)
        emit_palettes(pals, f)

    with open(os.path.join(ASSETS, "img_heart.c"), "w") as f:
        f.write(c_header())
        emit_true_color_alpha("img_heart", heart, f)

    logo = load_logo()
    splash = logo.resize((180, 180), Image.LANCZOS)
    with open(os.path.join(ASSETS, "img_logo.c"), "w") as f:
        f.write(c_header())
        emit_true_color_alpha("img_logo", splash, f)

    with open(os.path.join(ASSETS, "assets.h"), "w") as f:
        f.write("#pragma once\n#include <lvgl.h>\n\n"
                "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")
        for n in names:
            f.write(f"extern const lv_img_dsc_t img_baby_{n};\n")
        f.write("extern const lv_img_dsc_t img_heart;\n")
        f.write("extern const lv_img_dsc_t img_logo;\n\n")
        f.write(f"#define BABY_SKIN_TONE_COUNT {len(pals)}\n")
        f.write(f"extern const uint8_t baby_skin_palettes[{len(pals)}][256 * 4];\n\n")
        f.write("#ifdef __cplusplus\n}\n#endif\n")
    print("C assets written to src/assets/")


if __name__ == "__main__":
    main()
