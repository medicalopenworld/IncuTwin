#!/usr/bin/env python3
"""Assets adicionales (LVGL 9): marca IncuTwin (del logo), iconos de estado (termometro,
lampara), pollito del historial, iconos de cobertura WiFi (0..3 barras + sin WiFi) y las imagenes a pantalla
completa en vertical 240x320: incubadora vacia, bebe con sus padres (nino y nina, y en
piel oscura), corazon de IncuNest al terminar "Coge mi mano" la IncuNest pequena (mapa e
historial) y la cabeza del bebe, nino y nina (icono de sus avisos).
Uso: python tools/gen_extra.py"""
import colorsys
import os
import sys
from collections import deque

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageOps

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
IMAGES = os.path.join(ROOT, "Images")
sys.path.insert(0, HERE)
from gen_assets import (ASSETS, GIRL_HUE, GIRL_SAT_GAIN, PREVIEW, draw_bow,  # noqa: E402
                        load_baby, load_logo)
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


def make_chick(size):
    """Pollito amarillo asomando de media cascara blanca con borde en zigzag, ojo y pico
    naranja. Se dibuja en una cuadricula de 128 y se reduce con antialiasing; formas gruesas
    y simples para que se lea a 28 px, con contorno marron para separarlo del fondo claro."""
    k = 8
    big = 128 * k
    u = float(k)
    yellow, orange = (246, 196, 67, 255), (242, 140, 40, 255)
    shell, edge, ink = (255, 255, 255, 255), (140, 104, 56, 255), (40, 30, 20, 255)
    ow = 5 * u

    def silueta(draw, fill, grow=0.0):
        """Cabeza (circulo + copete) y cuenco de cascara; `grow` engorda para el contorno."""
        g = grow
        draw.ellipse((64 * u - 46 * u - g, 8 * u - g, 64 * u + 46 * u + g, 100 * u + g), fill=fill)
        draw.polygon([(54 * u - g, 14 * u), (62 * u, 0 - g), (74 * u + g, 14 * u)], fill=fill)

    # mascara de la cascara: elipse inferior recortada por arriba con zigzag
    top, n, x0, x1 = 86, 6, 4, 124
    zig = [(x0, 140)]
    for i in range(n):
        xa = x0 + (x1 - x0) * i / n
        xb = x0 + (x1 - x0) * (i + 0.5) / n
        zig += [(xa, top + 8), (xb, top - 8)]
    zig += [(x1, top + 8), (x1, 140)]
    keep = Image.new("L", (big, big), 0)
    ImageDraw.Draw(keep).polygon([(x * u, y * u) for x, y in zig], fill=255)
    bowl = Image.new("L", (big, big), 0)
    ImageDraw.Draw(bowl).ellipse((x0 * u, 58 * u, x1 * u, 126 * u), fill=255)
    shell_mask = ImageChops.multiply(keep, bowl)
    ring = shell_mask.filter(ImageFilter.MaxFilter(2 * int(ow) + 1))

    im = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    silueta(d, edge, grow=ow)           # contorno del pollito
    im.paste(Image.new("RGBA", im.size, edge), mask=ring)   # contorno de la cascara
    silueta(d, yellow)
    # pico (por encima de la cascara) y ojos
    d.polygon([(50 * u, 56 * u), (78 * u, 56 * u), (64 * u, 78 * u)], fill=orange,
              outline=edge, width=int(2 * u))
    for ex in (42 * u, 86 * u):
        d.ellipse((ex - 7 * u, 38 * u, ex + 7 * u, 52 * u), fill=ink)
        d.ellipse((ex - 3 * u, 40 * u, ex + 1 * u, 44 * u), fill=(255, 255, 255, 255))
    im.paste(Image.new("RGBA", im.size, shell), mask=shell_mask)
    return im.resize((size, size), Image.LANCZOS)


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


def make_flag_cm():
    """Bandera de Camerun (verde | rojo con estrella amarilla | amarillo), 96x64, esquinas
    redondeadas y borde fino, para la pagina del pais de la demo."""
    import math
    s = 4
    W, H = 96, 64
    im = Image.new("RGBA", (W * s, H * s), (0, 0, 0, 0))
    mask = Image.new("L", im.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, W * s - 1, H * s - 1), radius=6 * s, fill=255)
    d = ImageDraw.Draw(im)
    w3 = W * s / 3
    d.rectangle((0, 0, w3, H * s), fill=(0, 122, 94))
    d.rectangle((w3, 0, 2 * w3, H * s), fill=(206, 17, 38))
    d.rectangle((2 * w3, 0, W * s, H * s), fill=(252, 209, 22))
    cx, cy, ro, ri = W * s / 2, H * s / 2, 11 * s, 4.4 * s
    pts = []
    for k in range(10):
        r = ro if k % 2 == 0 else ri
        a = -math.pi / 2 + k * math.pi / 5
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    d.polygon(pts, fill=(252, 209, 22))
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    out.paste(im, mask=mask)
    ImageDraw.Draw(out).rounded_rectangle((0, 0, W * s - 1, H * s - 1), radius=6 * s,
                                          outline=(200, 200, 200, 255), width=s)
    return out.resize((W, H), Image.LANCZOS)


# Nina en Baby_with_parents.png (coordenadas del PNG original, 896x1199). El padre lleva
# camiseta azul y la madre una casi blanca pegadas al bebe: en vez de recolorear por zona se
# rellena desde semillas dentro del gorro y la manta, y el contorno marino del dibujo hace de
# frontera (comprobado: ninguna de las dos camisetas cambia).
PARENTS_GIRL_SEEDS = ((470, 665), (440, 660), (500, 650), (520, 680), (480, 630), (460, 640),
                      (340, 810), (430, 810), (480, 860), (380, 900), (450, 760), (300, 900))
PARENTS_GIRL_BOX = (200, 540, 640, 1040)
PARENTS_GIRL_VMIN = 0.40
PARENTS_BOW = (522, 598, 5)   # centro x, arriba y, celda: tapa el pompon (~523, 615)


def parents_girl(im):
    """Baby_with_parents.png -> la misma escena con la bebe en version nina (propuesta B)."""
    im = im.copy()
    px = im.load()
    x0, y0, x1, y1 = PARENTS_GIRL_BOX

    def bluish(p):
        hh, ss, vv = colorsys.rgb_to_hsv(*(c / 255 for c in p[:3]))
        return 0.50 <= hh <= 0.68 and ss > 0.06 and vv > PARENTS_GIRL_VMIN

    seen = set()
    dq = deque(PARENTS_GIRL_SEEDS)
    while dq:
        x, y = dq.popleft()
        if (x, y) in seen or not (x0 <= x < x1 and y0 <= y < y1) or not bluish(px[x, y]):
            continue
        seen.add((x, y))
        dq.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    for x, y in seen:
        hh, ss, vv = colorsys.rgb_to_hsv(*(c / 255 for c in px[x, y][:3]))
        r, g, b = colorsys.hsv_to_rgb(GIRL_HUE / 360, min(1, ss * GIRL_SAT_GAIN), vv)
        px[x, y] = (round(r * 255), round(g * 255), round(b * 255))
    cx, top, k = PARENTS_BOW
    draw_bow(ImageDraw.Draw(im), cx, top, k)
    return im


def fullscreen_from(png, transform=None):
    """Imagen derecha en la pantalla vertical: recorta margenes blancos y rellena a 240x320.
    `transform` se aplica al PNG original antes de recortar (la caja sale del original)."""
    src = Image.open(os.path.join(IMAGES, png)).convert("RGB")
    im = transform(src) if transform else src
    bg = Image.new("RGB", src.size, (255, 255, 255))
    diff = ImageChops.difference(src, bg).convert("L")
    bbox = diff.point(lambda v: 255 if v > 16 else 0).getbbox()
    im = im.crop(bbox)
    return ImageOps.pad(im, (SCREEN_W, SCREEN_H), Image.LANCZOS, color=(255, 255, 255))


def incunest_cutout():
    """IncuNest_empty.png sin el fondo blanco: se rellena el blanco desde los bordes (el blanco
    de dentro de la incubadora se queda) y se recorta al dibujo."""
    src = Image.open(os.path.join(IMAGES, "IncuNest_empty.png")).convert("RGBA")
    w, h = src.size
    px = src.load()
    bg = Image.new("L", src.size, 0)
    bpx = bg.load()

    def white(x, y):
        r, g, b, a = px[x, y]
        return a < 40 or (r > 235 and g > 235 and b > 235)

    dq = deque([(x, 0) for x in range(w)] + [(x, h - 1) for x in range(w)] +
               [(0, y) for y in range(h)] + [(w - 1, y) for y in range(h)])
    while dq:
        x, y = dq.popleft()
        if x < 0 or y < 0 or x >= w or y >= h or bpx[x, y] or not white(x, y):
            continue
        bpx[x, y] = 255
        dq.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    alpha = ImageOps.invert(bg).filter(ImageFilter.GaussianBlur(1.2))
    out = src.copy()
    out.putalpha(alpha)
    return out.crop(alpha.point(lambda v: 255 if v > 24 else 0).getbbox())


def make_incunest_icon(cut, width, halo):
    """IncuNest pequena: `width` px de ancho; con `halo` px de borde blanco para el mapa."""
    ratio = width / cut.width
    im = cut.resize((width, max(1, round(cut.height * ratio))), Image.LANCZOS)
    if not halo:
        return im
    W, H = im.width + 2 * halo, im.height + 2 * halo
    a = Image.new("L", (W, H), 0)
    a.paste(im.getchannel("A"), (halo, halo))
    a = a.point(lambda v: 255 if v > 60 else 0).filter(ImageFilter.MaxFilter(2 * halo + 1))
    a = a.filter(ImageFilter.GaussianBlur(0.6))
    out = Image.new("RGBA", (W, H), (255, 255, 255, 0))
    out.putalpha(a)
    out.alpha_composite(im, (halo, halo))
    return out


def make_baby_head(girl, width):
    """Cabeza del bebe (gorro y cara) para el icono de los avisos: nino (gorro azul) o nina
    (gorro rosa con lazo). El sprite del bebe recortado por arriba, a `width` px de ancho."""
    full, _ = load_baby(girl)
    head = full.crop((0, 0, full.width, round(full.height * 0.56)))
    head = head.crop(head.getchannel("A").point(lambda v: 255 if v > 24 else 0).getbbox())
    ratio = width / head.width
    return head.resize((width, max(1, round(head.height * ratio))), Image.LANCZOS)


def make_garland():
    """Guirnalda de Navidad para lo alto de Inicio (240x22): un cordon verde que cuelga en tres
    curvas entre cuatro lazos rojos, con bombillas de colores de la marca. Va detras de la marca y
    del boton del pais."""
    import math
    s = 4
    W, H = 240, 22
    im = Image.new("RGBA", (W * s, H * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    green = (46, 125, 80, 255)
    bulbs = [(222, 111, 99, 255), (245, 169, 59, 255), (5, 78, 146, 255), (63, 158, 99, 255)]
    anchors = [0, 80, 160, 240]
    sag = 11
    k = 0
    for a, b in zip(anchors, anchors[1:]):
        pts = []
        for i in range(41):
            t = i / 40
            x = a + (b - a) * t
            y = 3 + sag * math.sin(math.pi * t)
            pts.append((x * s, y * s))
        d.line(pts, fill=green, width=2 * s, joint="curve")
        for t in (0.2, 0.4, 0.6, 0.8):
            x = a + (b - a) * t
            y = 3 + sag * math.sin(math.pi * t)
            c = bulbs[k % len(bulbs)]
            k += 1
            d.rectangle(((x - 1.2) * s, (y + 0.5) * s, (x + 1.2) * s, (y + 2.5) * s), fill=(120, 120, 120, 255))
            d.ellipse(((x - 2.6) * s, (y + 2) * s, (x + 2.6) * s, (y + 8) * s), fill=c)
            d.ellipse(((x - 1.4) * s, (y + 3) * s, (x - 0.2) * s, (y + 4.6) * s), fill=(255, 255, 255, 160))
    for x in anchors:  # lazos
        x = min(max(x, 6), W - 6)
        d.polygon([(x * s, 4 * s), ((x - 6) * s, 0), ((x - 6) * s, 8 * s)], fill=(206, 43, 55, 255))
        d.polygon([(x * s, 4 * s), ((x + 6) * s, 0), ((x + 6) * s, 8 * s)], fill=(206, 43, 55, 255))
        d.ellipse(((x - 2) * s, 2 * s, (x + 2) * s, 6 * s), fill=(160, 20, 30, 255))
    return im.resize((W, H), Image.LANCZOS)


def save_chick_preview(c28, c64):
    """tools/preview/chick.png: ambos tamanos sobre el fondo claro de la UI, 28 px a x1 y x4
    (como se vera en pantalla y ampliado) y a 50 % de opacidad (pestana inactiva)."""
    bg = (244, 247, 251, 255)
    sheet = Image.new("RGBA", (420, 140), bg)
    sheet.alpha_composite(c28, (10, 10))
    faded = c28.copy()
    faded.putalpha(faded.getchannel("A").point(lambda v: v // 2))
    sheet.alpha_composite(faded, (50, 10))
    sheet.alpha_composite(c28.resize((112, 112), Image.NEAREST), (100, 14))
    sheet.alpha_composite(c64, (230, 10))
    sheet.alpha_composite(c64.resize((128, 128), Image.LANCZOS), (290, 6))
    sheet.save(os.path.join(PREVIEW, "chick.png"))


def main():
    os.makedirs(PREVIEW, exist_ok=True)
    os.makedirs(os.path.join(ASSETS, "include"), exist_ok=True)
    wm = make_wordmark()
    th = make_thermo()
    lp = make_lamp()
    chick28 = make_chick(28)
    chick64 = make_chick(64)
    wifis = [make_wifi(i) for i in range(4)]
    wifi_off = make_wifi_off()
    empty = fullscreen_from("IncuNest_empty.png")
    parents = fullscreen_from("Baby_with_parents.png")
    parents_black = fullscreen_from("Baby_with_parents_black.jpg")  # tonos de piel 3..5
    thanks = fullscreen_from("Corazon_IncuNest.png")  # fin de "Coge mi mano"
    flag_cm = make_flag_cm()  # pagina del pais en la demo
    parents_g = fullscreen_from("Baby_with_parents.png", parents_girl)
    nest = incunest_cutout()
    nest_map = make_incunest_icon(nest, 40, 2)  # mapa: la IncuNest que vuelve al bebe
    nest_28 = make_incunest_icon(nest, 28, 0)   # historial: cabecera de cada incubadora
    head_boy = make_baby_head(False, 26)       # avisos del bebe: nino (o sin sexo)
    head_girl = make_baby_head(True, 26)       # avisos del bebe: nina
    garland = make_garland()                    # Navidad en lo alto de Inicio

    wm.save(os.path.join(PREVIEW, "wordmark.png"))
    th.save(os.path.join(PREVIEW, "icon_thermo.png"))
    lp.save(os.path.join(PREVIEW, "icon_lamp.png"))
    wifi_off.save(os.path.join(PREVIEW, "wifi_off.png"))
    chick28.save(os.path.join(PREVIEW, "chick_28.png"))
    chick64.save(os.path.join(PREVIEW, "chick_64.png"))
    save_chick_preview(chick28, chick64)
    for i, w in enumerate(wifis):
        w.save(os.path.join(PREVIEW, f"wifi_{i}.png"))
    empty.save(os.path.join(PREVIEW, "incunest_empty.png"))
    parents.save(os.path.join(PREVIEW, "baby_parents.png"))
    parents_black.save(os.path.join(PREVIEW, "baby_parents_black.png"))
    thanks.save(os.path.join(PREVIEW, "hand_thanks.png"))
    flag_cm.save(os.path.join(PREVIEW, "flag_cm.png"))
    parents_g.save(os.path.join(PREVIEW, "baby_parents_girl.png"))
    nest_map.save(os.path.join(PREVIEW, "incunest_map.png"))
    nest_28.save(os.path.join(PREVIEW, "incunest_28.png"))
    print("incunest_map:", nest_map.size, " incunest_28:", nest_28.size)
    head_boy.save(os.path.join(PREVIEW, "baby_head_boy.png"))
    head_girl.save(os.path.join(PREVIEW, "baby_head_girl.png"))
    garland.save(os.path.join(PREVIEW, "garland.png"))
    print("baby_head:", head_boy.size, head_girl.size)
    print("wordmark:", wm.size, " empty:", empty.size)

    with open(os.path.join(ASSETS, "img_extra.c"), "w", encoding="utf-8", newline="\n") as f:
        f.write(c_header("Marca, iconos de estado, cobertura WiFi e imagenes a pantalla completa"))
        emit_argb8888(f, "img_wordmark", wm)
        emit_argb8888(f, "img_icon_thermo", th)
        emit_argb8888(f, "img_icon_lamp", lp)
        emit_argb8888(f, "img_wifi_off", wifi_off)
        emit_argb8888(f, "img_chick_28", chick28)
        emit_argb8888(f, "img_chick_64", chick64)
        for i, w in enumerate(wifis):
            emit_argb8888(f, f"img_wifi_{i}", w)
        emit_rgb565(f, "img_incunest_empty", empty)
        emit_rgb565(f, "img_baby_parents", parents)
        emit_rgb565(f, "img_baby_parents_black", parents_black)
        emit_rgb565(f, "img_hand_thanks", thanks)
        emit_argb8888(f, "img_flag_cm", flag_cm)
        emit_rgb565(f, "img_baby_parents_girl", parents_g)
        emit_argb8888(f, "img_incunest_map", nest_map)
        emit_argb8888(f, "img_incunest_28", nest_28)
        emit_argb8888(f, "img_baby_head_boy", head_boy)
        emit_argb8888(f, "img_baby_head_girl", head_girl)
        emit_argb8888(f, "img_garland", garland)

    write_header(
        os.path.join(ASSETS, "include", "assets_extra.h"),
        "Assets adicionales (gen_extra.py)",
        ["img_wordmark", "img_icon_thermo", "img_icon_lamp", "img_wifi_off",
         "img_chick_28", "img_chick_64",
         "img_wifi_0", "img_wifi_1", "img_wifi_2", "img_wifi_3",
         "img_incunest_empty", "img_baby_parents", "img_baby_parents_black", "img_hand_thanks",
         "img_flag_cm", "img_baby_parents_girl", "img_incunest_map", "img_incunest_28",
         "img_baby_head_boy", "img_baby_head_girl", "img_garland"],
    )
    print("img_extra.c escrito")


if __name__ == "__main__":
    main()
