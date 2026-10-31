#!/usr/bin/env python3
"""
Generador de assets del bebe para IncuTwin (LVGL 9).

A partir de Images/Baby.png e Images/IncuTwin_logo.png genera en components/assets/:
  - img_baby.c  : fotogramas sleep, yawn1, yawn2, awake como I8 con paleta comun,
                  y las 6 paletas de tono de piel (solo cambian las entradas de piel).
  - img_heart.c : corazon (ARGB8888).
  - img_logo.c  : logo del splash (ARGB8888).
  - include/assets_baby.h

Uso:  python tools/gen_assets.py [--preview-only]
Previsualizaciones en tools/preview/.
"""
import os
import sys
from collections import deque

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
IMAGES = os.path.join(ROOT, "Images")
PREVIEW = os.path.join(HERE, "preview")
ASSETS = os.path.join(ROOT, "components", "assets")
sys.path.insert(0, HERE)
from lvgl9_c import c_header, emit_argb8888, emit_i8, emit_palettes, write_header  # noqa: E402

BABY_H = 185          # alto final del sprite del bebe en px
SKIN_BASE = (244, 193, 166)   # piel original de Baby.png
BLUSH_BASE = (239, 148, 146)  # rubor aproximado
NAVY = (21, 38, 82)
MOUTH_FILL = (156, 88, 96)
TONGUE = (232, 150, 150)

# 6 tonos de piel configurables (piel, rubor)
SKIN_TONES = [
    ((244, 193, 166), (239, 148, 146)),  # 0 claro (original)
    ((233, 175, 140), (226, 136, 130)),  # 1 medio claro
    ((208, 148, 108), (198, 116, 104)),  # 2 medio
    ((172, 112, 78),  (168, 92, 84)),    # 3 bronceado
    ((134, 84, 56),   (140, 74, 68)),    # 4 moreno
    ((92, 58, 40),    (104, 56, 52)),    # 5 oscuro
]


def is_skin(p):
    r, g, b = p[:3]
    # piel = calida Y claramente r>g (las lunas amarillentas de la manta tienen r~g)
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

    # relleno desde los bordes: el fondo blanco pasa a transparente
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
    return im.resize((round(im.width * ratio), BABY_H), Image.LANCZOS)


def face_regions(im):
    """Caja de la cara a partir de los pixeles de piel."""
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
    """Devuelve dict nombre -> fotograma RGBA."""
    fx0, fx1, fy0, fy1 = face_regions(base)
    fw = fx1 - fx0
    fh = fy1 - fy0
    cx = (fx0 + fx1) // 2
    mouth_y = fy0 + int(fh * 0.72)

    frames = {"sleep": base.copy()}

    # bostezo: tapar la sonrisa y dibujar la boca abierta
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

    # despierto: tapar los arcos de ojos cerrados y dibujar ojos redondos
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
    return im.resize((W, H), Image.LANCZOS)


def quantize_frames(frames, ncolors=48):
    """Cuantizacion conjunta de todos los fotogramas: paleta compartida."""
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
    palette.insert(0, (0, 0, 0, 0))  # indice 0 = transparente
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
    os.makedirs(os.path.join(ASSETS, "include"), exist_ok=True)

    base = load_baby()
    frames = make_frames(base)
    heart = make_heart()

    palette, idxmaps, size = quantize_frames(frames)
    sidx = skin_indices(palette)
    pals = build_tone_palettes(palette, sidx)

    # previsualizaciones
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
    print(f"frames: {names}  size={size}  palette={len(palette)} skin_entries={len(sidx)}")
    if preview_only:
        return

    # salida C
    with open(os.path.join(ASSETS, "img_baby.c"), "w", encoding="utf-8", newline="\n") as f:
        f.write(c_header("Bebe animado: fotogramas I8 con paleta comun + paletas de tono de piel"))
        for n in names:
            emit_i8(f, f"img_baby_{n}", palette, idxmaps[n], size)
        emit_palettes(f, "baby_skin_palettes", pals)

    with open(os.path.join(ASSETS, "img_heart.c"), "w", encoding="utf-8", newline="\n") as f:
        f.write(c_header("Corazon del bebe"))
        emit_argb8888(f, "img_heart", heart)

    splash = load_logo().resize((180, 180), Image.LANCZOS)
    with open(os.path.join(ASSETS, "img_logo.c"), "w", encoding="utf-8", newline="\n") as f:
        f.write(c_header("Logo IncuTwin para el splash"))
        emit_argb8888(f, "img_logo", splash)

    write_header(
        os.path.join(ASSETS, "include", "assets_baby.h"),
        "Assets del bebe (gen_assets.py)",
        [f"img_baby_{n}" for n in names] + ["img_heart", "img_logo"],
        extra=(f"#define BABY_SKIN_TONE_COUNT {len(pals)}\n"
               f"#define BABY_PALETTE_BYTES (256 * 4)\n"
               f"extern const uint8_t baby_skin_palettes[{len(pals)}][256 * 4];\n"),
    )
    print(f"C escrito en {ASSETS}")


if __name__ == "__main__":
    main()
