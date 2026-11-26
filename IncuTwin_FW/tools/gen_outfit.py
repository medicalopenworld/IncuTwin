"""Gorros y mantas del bebe para la tienda (cambio openspec `tienda`). Lo usa gen_assets.py.

- La mascara del gorro sale de Images/Baby.png rellenando desde dentro del gorro, con el contorno
  marino como muro: asi la capucha de la manta (que rodea al gorro) no cuenta como gorro.
- Tras cuantizar, los pixeles recoloreables (azules del nino, rosas de la nina, con brillo) del
  gorro y de la manta pasan a entradas de paleta propias (copias). La cara, el contorno, el pompon
  y el lazo no se tocan.
- Cada estilo de la tienda es una lista de colores para esas entradas: el panel solo copia colores
  en la paleta (cero bytes de imagen nuevos).
- Papa Noel: sprite ARGB dibujado en pixeles al tamaño de celda de Baby.png y escalado igual que el
  bebe, con su posicion relativa al sprite del bebe.
"""
import colorsys
import math
from collections import deque

from PIL import Image, ImageDraw

GIRL_SAT_GAIN = 1.5  # el mismo que gen_assets.girl_recolor

# (nombre, (tono en grados, ganancia de saturacion o None, saturacion fija o None)); None = tal cual.
# El orden es el de BABY_HAT_* / BABY_BLANKET_* en assets_baby.h.
HAT_STYLES = [
    ("BASIC", None),
    ("MINT", (150, 1.0, None)),
    ("CHICK", (48, 1.6, None)),
    ("CLOUD", (220, None, 0.05)),
    ("LILAC", (275, 1.3, None)),
    ("PEACH", (22, 1.4, None)),
]
BLANKET_STYLES = [
    ("BASIC", None),
    ("CREAM", (42, 0.9, None)),
    ("MINT", (150, 1.0, None)),
    ("LAVENDER", (265, 1.2, None)),
    ("PEACH", (20, 1.3, None)),
]

NAVY = (21, 38, 82, 255)
CELL = 7  # un pixel del dibujo en Baby.png


def kind_of(rgb):
    """'blue' (nino), 'pink' (nina) o None si el color no se recolorea."""
    h, s, v = colorsys.rgb_to_hsv(rgb[0] / 255, rgb[1] / 255, rgb[2] / 255)
    if v <= 0.62 or s <= 0.06:
        return None
    if 0.50 <= h <= 0.68:
        return "blue"
    if 0.93 <= h <= 0.975 and s <= 0.9:  # el rosa de girl_recolor (0.96); el lazo (rojo, 0.98) no
        return "pink"
    return None


def restyle(rgb, style):
    if style is None:
        return tuple(rgb[:3])
    hue, gain, sset = style
    h, s, v = colorsys.rgb_to_hsv(rgb[0] / 255, rgb[1] / 255, rgb[2] / 255)
    if kind_of(rgb) == "pink":
        s = s / GIRL_SAT_GAIN  # volver al azul de partida
    s2 = sset if sset is not None else min(1.0, s * gain)
    r, g, b = colorsys.hsv_to_rgb(hue / 360, s2, v)
    return (round(r * 255), round(g * 255), round(b * 255))


def hat_mask_source(src):
    """Mascara (bytearray w*h) del gorro en Baby.png."""
    w, h = src.size
    px = src.load()
    seen = bytearray(w * h)
    dq = deque((x, y) for x in range(400, 500, 6) for y in (440, 470, 495, 505, 515))
    while dq:
        x, y = dq.popleft()
        if x < 0 or y < 330 or x >= w or y >= 530 or seen[y * w + x]:
            continue
        r, g, b, a = px[x, y]
        if a == 0:
            continue
        hh, ss, vv = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
        if vv < 0.5:
            continue  # contorno marino = muro
        if hh < 0.4 and ss > 0.2:
            continue  # piel / pompon
        seen[y * w + x] = 1
        dq.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    return seen


def hat_mask_sprite(src, bbox, size):
    """La mascara del gorro llevada al sprite (mismo recorte y escala que load_baby)."""
    w, h = src.size
    m = Image.new("L", (w, h), 0)
    m.putdata([255 if v else 0 for v in hat_mask_source(src)])
    m = m.crop(bbox).resize(size, Image.BOX)
    return [1 if v >= 128 else 0 for v in m.getdata()]


def face_mask(idxmaps, size, skin_idx):
    """La cara: en cada fila, de la primera a la ultima piel (ojos y boca dentro; la manta de los
    lados, fuera), en cualquier fotograma."""
    w, h = size
    skin = set(skin_idx)
    mask = bytearray(w * h)
    for v in idxmaps.values():
        for y in range(h):
            xs = [x for x in range(w) if v[y * w + x] in skin]
            if xs:
                for x in range(xs[0], xs[-1] + 1):
                    mask[y * w + x] = 1
    return mask


def bow_mask_sprite(src_size, bbox, size, draw_bow):
    """El lazo de la nina (gen_assets.draw_bow) con un pixel de margen, en el sprite: ni gorro ni manta."""
    m = Image.new("RGBA", src_size, (0, 0, 0, 0))
    draw_bow(ImageDraw.Draw(m), 447, 380, 8)
    a = m.getchannel("A").crop(bbox).resize(size, Image.BOX)
    w, h = size
    on = [1 if v else 0 for v in a.getdata()]
    out = bytearray(w * h)
    for pos, v in enumerate(on):
        if v:
            x, y = pos % w, pos // w
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if 0 <= x + dx < w and 0 <= y + dy < h:
                        out[(y + dy) * w + x + dx] = 1
    return out


def split_outfit(palette, idxmaps, size, hat_mask, face, bow=None):
    """Mueve los pixeles recoloreables del gorro y de la manta a entradas de paleta propias.
    `face`: face_mask(). Modifica `palette` e `idxmaps`. Devuelve (indices del gorro, de la manta)."""
    kinds = [kind_of(c[:3]) if c[3] else None for c in palette]
    hat_map, blk_map = {}, {}
    for name, v in idxmaps.items():
        girl = name.startswith("girl_")
        for pos, i in enumerate(v):
            if not kinds[i] or (girl and bow and bow[pos]):
                continue
            if hat_mask[pos]:
                m = hat_map
            elif face[pos]:
                continue  # brillo de los ojos y demas de la cara
            else:
                m = blk_map
            if i not in m:
                m[i] = len(palette)
                palette.append(palette[i])
            v[pos] = m[i]
    assert len(palette) <= 256, len(palette)
    return sorted(hat_map.values()), sorted(blk_map.values())


def style_table(palette, idx, styles):
    return [[restyle(palette[i], st) + (255,) for i in idx] for _, st in styles]


def _outline(low):
    w, h = low.size
    px = low.load()
    out = low.copy()
    po = out.load()
    for y in range(h):
        for x in range(w):
            if px[x, y][3]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                xx, yy = x + dx, y + dy
                if 0 <= xx < w and 0 <= yy < h and px[xx, yy][3] and px[xx, yy][:3] != NAVY[:3]:
                    po[x, y] = NAVY
                    break
    return out


def santa_source(size):
    """Gorro de Papa Noel en pixeles, colocado sobre Baby.png (tapa el pompon y el lazo)."""
    red, dark, white, fur = (214, 64, 74, 255), (176, 44, 62, 255), (250, 248, 244, 255), (214, 222, 234, 255)
    low = Image.new("RGBA", (40, 33), (0, 0, 0, 0))
    d = ImageDraw.Draw(low)
    d.polygon([(2, 26), (5, 16), (10, 9), (16, 5), (23, 4), (29, 6), (34, 11), (34, 13), (31, 13), (27, 10),
               (28, 18), (29, 26)], fill=red)
    d.polygon([(2, 26), (5, 16), (10, 9), (12, 9), (8, 17), (7, 26)], fill=dark)
    d.polygon([(23, 4), (29, 6), (34, 11), (34, 13), (31, 13), (27, 9), (24, 7)], fill=dark)
    d.rectangle((0, 25, 31, 31), fill=white)
    d.rectangle((0, 30, 31, 31), fill=fur)
    for x in range(1, 31, 4):
        d.point((x, 27), fill=fur)
    d.ellipse((31, 11, 38, 18), fill=white)
    for p in ((35, 17), (36, 16), (34, 17)):
        d.point(p, fill=fur)
    low = _outline(low)
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.alpha_composite(low.resize((low.width * CELL, low.height * CELL), Image.NEAREST), (338, 532 - 33 * CELL))
    return canvas


def santa_sprite(src_size, bbox, sprite_size):
    """(imagen, x, y): Papa Noel a la escala del sprite y su posicion relativa al sprite."""
    canvas = santa_source(src_size)
    sb = canvas.getbbox()
    rx = sprite_size[0] / (bbox[2] - bbox[0])
    ry = sprite_size[1] / (bbox[3] - bbox[1])
    sx0 = math.floor((sb[0] - bbox[0]) * rx)
    sy0 = math.floor((sb[1] - bbox[1]) * ry)
    sx1 = math.ceil((sb[2] - bbox[0]) * rx)
    sy1 = math.ceil((sb[3] - bbox[1]) * ry)
    box = (bbox[0] + sx0 / rx, bbox[1] + sy0 / ry, bbox[0] + sx1 / rx, bbox[1] + sy1 / ry)
    img = canvas.resize((sx1 - sx0, sy1 - sy0), Image.LANCZOS, box=box)
    return img, sx0, sy0


def emit_tables(f, hat_idx, hat_tab, blk_idx, blk_tab):
    def arr(name, idx, tab):
        f.write(f"const uint8_t {name}_idx[{len(idx)}] = {{ {', '.join(str(i) for i in idx)} }};\n")
        f.write(f"const uint8_t {name}_colors[{len(tab)}][{len(idx)}][4] = {{\n")
        for row in tab:
            f.write("  { " + ", ".join(f"{{0x{b:02x},0x{g:02x},0x{r:02x},0x{a:02x}}}" for r, g, b, a in row) + " },\n")
        f.write("};\n\n")
    arr("baby_hat", hat_idx, hat_tab)
    arr("baby_blanket", blk_idx, blk_tab)


def header_extra(hat_idx, blk_idx, santa_xy):
    s = "/* Tienda: estilos de gorro y manta (entradas de paleta y sus colores, B,G,R,A) */\n"
    for i, (n, _) in enumerate(HAT_STYLES):
        s += f"#define BABY_HAT_{n} {i}\n"
    s += f"#define BABY_HAT_STYLE_COUNT {len(HAT_STYLES)}\n#define BABY_HAT_N_IDX {len(hat_idx)}\n"
    for i, (n, _) in enumerate(BLANKET_STYLES):
        s += f"#define BABY_BLANKET_{n} {i}\n"
    s += f"#define BABY_BLANKET_STYLE_COUNT {len(BLANKET_STYLES)}\n#define BABY_BLANKET_N_IDX {len(blk_idx)}\n"
    s += "extern const uint8_t baby_hat_idx[BABY_HAT_N_IDX];\n"
    s += "extern const uint8_t baby_hat_colors[BABY_HAT_STYLE_COUNT][BABY_HAT_N_IDX][4];\n"
    s += "extern const uint8_t baby_blanket_idx[BABY_BLANKET_N_IDX];\n"
    s += "extern const uint8_t baby_blanket_colors[BABY_BLANKET_STYLE_COUNT][BABY_BLANKET_N_IDX][4];\n"
    s += f"/* Papa Noel: esquina relativa al sprite del bebe */\n#define BABY_SANTA_X ({santa_xy[0]})\n#define BABY_SANTA_Y ({santa_xy[1]})\n"
    return s
