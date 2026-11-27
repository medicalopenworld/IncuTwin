#!/usr/bin/env python3
"""Bebe sin WiFi (nino y nina): animacion que se ve en Inicio cuando el panel no tiene WiFi.

Fuente: Images/Baby_no_wifi.png, una tira de 5 dibujos (chupete -> coge el chupete -> saca
la antena -> antena fuera, boca de "oh" -> antena arriba con el WiFi tachado). De ahi salen:
  - 5 cuerpos alineados por la manta (borde izquierdo a media altura y la base), recortados
    cada uno a su caja para no gastar flash en transparente;
  - el simbolo del WiFi del ultimo dibujo partido en piezas (punto, 3 arcos, aspa + rayo)
    para que aparezca por partes y parpadee: imagenes acumuladas que se ponen encima;
  - la version nina: gorro y manta rosas (girl_recolor de gen_assets) y lazo en el pompon.
La secuencia (fotograma, ms) va en SEQ y se escribe tambien en el .h para el firmware.

Uso: python tools/gen_nowifi.py            -> tools/preview/nowifi_*.gif / .png
     python tools/gen_nowifi.py --emit     -> ademas components/assets/img_nowifi.c y .h"""
import os
import sys
from collections import deque

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from gen_assets import ASSETS, IMAGES, PREVIEW, draw_bow, girl_recolor  # noqa: E402
from lvgl9_c import c_header, emit_argb8888, emit_i8, write_header  # noqa: E402

SRC = os.path.join(IMAGES, "Baby_no_wifi.png")
BG = (0xF4, 0xF3, 0xF0, 255)          # COL_BG de ui_theme.h
WHITE_MIN = 226                        # min(R,G,B) por encima = fondo blanco
SCALE = 0.49                           # 441 px de alto en la fuente -> ~229 en pantalla (debajo, el texto)

# Columnas de cada dibujo en la fuente (1200x896) y su ancla: borde izquierdo de la manta a
# y = 600 y la base del dibujo. Medidos sobre la imagen.
FRAMES = [  # x0, x1, ancla_x, base_y
    (31, 242, 48, 660),
    (275, 486, 291, 664),
    (497, 707, 509, 665),
    (713, 927, 730, 664),
    (936, 1185, 959, 665),
]
# Simbolo del WiFi (ultimo dibujo): por encima de la bola de la antena (y < 279) o, a la
# derecha de la antena, el rayo (x > 1138, y < 305).
WIFI_DOT = (1120, 267)


def in_symbol(x, y):
    return y < 279 or (x > 1138 and y < 305)


def symbol_part(x, y):
    """Pieza del simbolo a la que pertenece un pixel de la fuente."""
    if x > 1157 and y < 250:
        return "x"          # aspa
    if x > 1136 and y >= 246:
        return "x"          # rayo (va con el aspa)
    dx, dy = x - WIFI_DOT[0], y - WIFI_DOT[1]
    r = (dx * dx + dy * dy) ** 0.5
    if r < 8:
        return "dot"
    if r < 20:
        return "arc1"
    if r < 31:
        return "arc2"
    return "arc3"


ARCS = ("dot", "arc1", "arc2", "arc3")
# Cada paso: piezas que se ven; las de NAVY_PARTS se pintan en azul de la marca (conectado) y
# "ok" es una marca de verificacion verde donde estaba el aspa.
SYM_STEPS = [
    ("dot",), ("dot", "arc1"), ("dot", "arc1", "arc2"), ARCS, ARCS + ("x",),     # 0..4 sin WiFi
    ARCS + ("dot+",), ARCS + ("dot+", "arc1+"), ARCS + ("dot+", "arc1+", "arc2+"),  # 5..7 se
    ARCS + ("dot+", "arc1+", "arc2+", "arc3+"),                                    # 8  enciende
    ARCS + ("dot+", "arc1+", "arc2+", "arc3+", "ok"),                              # 9  conectado
]
NAVY = (5, 78, 146)                    # COL_NAVY
OK_GREEN = (63, 158, 99)               # COL_OK
INK = (30, 62, 110)                    # COL_NAVY_DARK

# Secuencia: (cuerpo 0..4, simbolo -1..4 = SYM_STEPS acumulado, ms)
SEQ = [
    (0, -1, 700), (1, -1, 380), (0, -1, 380), (1, -1, 380), (0, -1, 520),   # chupete
    (1, -1, 300), (2, -1, 480),                                              # saca la antena
    (3, -1, 750),                                                            # "oh"
    (4, -1, 320), (4, 0, 220), (4, 1, 220), (4, 2, 220), (4, 3, 380),        # busca senal
    (4, 4, 520), (4, 3, 300), (4, 4, 520), (4, 3, 300), (4, 4, 1400),        # no hay
]

# Asistente (QR) y Cambiar WiFi, mientras prueba la red: saca la antena una vez y luego los
# arcos aparecen en bucle, sin aspa ni rayo (aun no ha fallado nada). Los pasos desde
# SEARCH_LOOP se repiten. (cuerpo, simbolo, ms)
SEARCH_SEQ = [
    (0, -1, 600), (1, -1, 350), (2, -1, 450), (3, -1, 650),                  # saca la antena
    (4, -1, 350), (4, 0, 260), (4, 1, 260), (4, 2, 260), (4, 3, 700),        # busca senal
]
SEARCH_LOOP = 4

# Celebracion al conectar (una vez, ~3,6 s; luego Inicio normal): se va el aspa, los arcos se
# encienden en azul, sale la marca verde y salta dos veces con la antena fuera ("¡oh!")
# mientras salen corazones, estrellas y confeti de la punta. (cuerpo, simbolo, ms, salto px)
HOP = [-5, -10, -12, -10, -5, 0]
CELEB_SEQ = (
    [(4, 3, 200, 0), (4, 5, 110, 0), (4, 6, 110, 0), (4, 7, 110, 0), (4, 8, 140, 0),
     (4, 9, 320, 0)]
    + [(3, -1, 80, dy) for dy in HOP]
    + [(4, 9, 80, dy) for dy in (-4, -7, -4, 0)]
    + [(3, -1, 80, dy) for dy in HOP]
    + [(4, 9, 80, dy) for dy in (-4, -7, -4, 0)]
    + [(4, 9, 1000, 0)]
)
CELEB_BURST = 1  # la explosion sale al empezar el primer salto (n.o de salto, desde 1)

# Particulas (en px de pantalla, salen de la punta de la antena del dibujo 4):
# (tipo, angulo en grados, velocidad px/s, retraso ms). Gravedad CELEB_G, vida CELEB_LIFE y
# se desvanecen los ultimos CELEB_FADE ms.
CELEB_G, CELEB_LIFE, CELEB_FADE = 520, 1500, 450
PARTICLES = [
    ("heart", -100, 190, 0), ("star", -60, 200, 0), ("heart", -135, 175, 40),
    ("conf_mint", -80, 230, 20), ("conf_peach", -40, 180, 60), ("star", -150, 160, 80),
    ("heart", -20, 150, 120), ("conf_navy", -115, 215, 100), ("conf_lilac", -165, 140, 140),
    ("heart", -70, 170, 180), ("star", -110, 150, 220), ("conf_mint", -30, 210, 200),
    ("conf_peach", -145, 200, 240), ("heart", -90, 140, 300), ("conf_lilac", -55, 240, 160),
    ("conf_navy", -10, 170, 260), ("star", -125, 220, 320), ("conf_mint", -175, 160, 300),
]
SPRITES = {
    "heart": [".NN...NN.",
              "NCCN.NCCN",
              "NCWCNCCCN",
              "NCCCCCCCN",
              ".NCCCCCN.",
              "..NCCCN..",
              "...NCN...",
              "....N...."],
    "star": ["....N....",
             "...NYN...",
             "NNNYYYNNN",
             "NYYYWYYYN",
             ".NYYYYYN.",
             "..NYYYN..",
             ".NYYNYYN.",
             ".NYN.NYN.",
             ".NN...NN."],
}
SPRITE_COLS = {"N": INK, "C": (222, 111, 99), "W": (255, 255, 255), "Y": (242, 182, 48)}
CONFETTI = {"conf_mint": (108, 195, 160), "conf_peach": (245, 169, 59),
            "conf_navy": NAVY, "conf_lilac": (183, 156, 224)}


def remove_white(im):
    """Fondo blanco -> transparente rellenando desde los bordes y desde los huecos blancos
    grandes (entre brazo, antena y gorro). Los blancos pequenos (ojos, brillos) se quedan."""
    im = im.convert("RGBA")
    w, h = im.size
    px = im.load()
    white = [[min(px[x, y][:3]) >= WHITE_MIN for x in range(w)] for y in range(h)]
    seen = [[False] * w for _ in range(h)]

    def fill(seeds):
        comp = []
        dq = deque(seeds)
        while dq:
            x, y = dq.popleft()
            if x < 0 or y < 0 or x >= w or y >= h or seen[y][x] or not white[y][x]:
                continue
            seen[y][x] = True
            comp.append((x, y))
            dq.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
        return comp

    border = [(x, 0) for x in range(w)] + [(x, h - 1) for x in range(w)] + \
             [(0, y) for y in range(h)] + [(w - 1, y) for y in range(h)]
    clear = fill(border)
    for y in range(h):
        for x in range(w):
            if white[y][x] and not seen[y][x]:
                comp = fill([(x, y)])
                if len(comp) > 260:      # un hueco de fondo, no un brillo
                    clear += comp
    for x, y in clear:
        px[x, y] = (255, 255, 255, 0)
    return im


def girlify(im, pompom):
    """Gorro y manta rosas y un lazo tapando el pompon (cx, top, celda) en px de la fuente."""
    im = im.copy()
    girl_recolor(im)
    cx, top, k = pompom
    draw_bow(ImageDraw.Draw(im), cx, top, k)
    return im


def find_pompom(src, x0, x1):
    """Pompon rojo: los pixeles rojos mas altos del gorro (y 340..400)."""
    px = src.load()
    pts = [(x, y) for y in range(340, 400) for x in range(x0, x1)
           if px[x, y][0] > 150 and px[x, y][1] < 90 and px[x, y][2] < 90]
    xs = sorted(p[0] for p in pts)
    ys = sorted(p[1] for p in pts)
    # el pompon es un bloque compacto: descarta mejillas (mas abajo) quedandose con lo alto
    top = ys[0]
    blob = [p for p in pts if p[1] < top + 30]
    bx = [p[0] for p in blob]
    by = [p[1] for p in blob]
    return (min(bx) + max(bx)) // 2, min(by), max(bx) - min(bx) + 1, max(by) - min(by) + 1


def build(girl):
    """Devuelve (cuerpos, simbolos): listas de (imagen RGBA a escala, x, y) en coordenadas
    del lienzo comun (origen arriba a la izquierda)."""
    src = Image.open(SRC).convert("RGB")
    raw = []
    for i, (x0, x1, ax, by) in enumerate(FRAMES):
        orig = src.crop((x0, 200, x1 + 1, by + 1))
        crop = orig
        if girl:
            pcx, ptop, pw, _ = find_pompom(src, x0, x1 + 1)
            k = 4  # lazo de 52x24 px: ~0,4 del ancho del gorro, como el del bebe de Inicio
            crop = girlify(orig, (pcx - x0, ptop - 200 - 3, k))
        im = remove_white(crop)
        if i == 4:  # el simbolo se dibuja aparte, y siempre con sus colores (sin el rosa)
            ref = remove_white(orig)
            parts = {}
            ipx, rpx = im.load(), ref.load()
            for y in range(im.height):
                for x in range(im.width):
                    gx, gy = x + x0, y + 200
                    if rpx[x, y][3] and in_symbol(gx, gy):
                        parts.setdefault(symbol_part(gx, gy), []).append((x, y, rpx[x, y]))
                        ipx[x, y] = (255, 255, 255, 0)
            sym_layers = parts
        raw.append((im, x0 - ax, 200 - by))  # offset del recorte respecto del ancla
    # lienzo comun: ancla en (-minx, -miny) tras escalar
    minx = min(dx for _, dx, _ in raw)
    miny = min(dy for _, _, dy in raw)
    bodies = []
    for im, dx, dy in raw:
        bb = im.getbbox()
        part = im.crop(bb)
        sw = max(1, round(part.width * SCALE))
        sh = max(1, round(part.height * SCALE))
        x = round((dx - minx + bb[0]) * SCALE)
        y = round((dy - miny + bb[1]) * SCALE)
        bodies.append((part.resize((sw, sh), Image.LANCZOS), x, y))
    # simbolos acumulados, recortados a la caja del simbolo completo
    w5, h5 = raw[4][0].size
    full = Image.new("RGBA", (w5, h5), (0, 0, 0, 0))
    for pts in sym_layers.values():
        for x, y, c in pts:
            full.putpixel((x, y), c)
    draw_check(full, FRAMES[4][0])  # la caja tiene que abarcar tambien la marca
    sbb = full.getbbox()
    syms = []
    for step in SYM_STEPS:
        layer = Image.new("RGBA", (w5, h5), (0, 0, 0, 0))
        for name in step:
            navy = name.endswith("+")
            if name == "ok":
                draw_check(layer, FRAMES[4][0])
                continue
            for x, y, c in sym_layers.get(name.rstrip("+"), []):
                layer.putpixel((x, y), to_navy(c) if navy else c)
        part = layer.crop(sbb)
        sw, sh = round(part.width * SCALE), round(part.height * SCALE)
        x = round((raw[4][1] - minx + sbb[0]) * SCALE)
        y = round((raw[4][2] - miny + sbb[1]) * SCALE)
        syms.append((part.resize((sw, sh), Image.LANCZOS), x, y))
    return bodies, syms


def to_navy(c):
    """Pixel del simbolo -> azul de la marca conservando la luz (bordes suaves mas claros)."""
    r, g, b, a = c
    lum = 0.299 * r + 0.587 * g + 0.114 * b
    k = min(1.0, max(0.0, (lum - 90) / (WHITE_MIN - 90))) * 0.85
    return tuple(round(n * (1 - k) + 255 * k) for n in NAVY) + (a,)


def draw_check(layer, x0):
    """Marca verde con contorno marino donde estaba el aspa (centro ~ (1172, 236) en la
    fuente), dibujada a 4x y reducida para que el trazo quede suave."""
    k = 4
    big = Image.new("RGBA", (layer.width * k, layer.height * k), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    pts = [(1160 - x0, 236 - 200), (1169 - x0, 246 - 200), (1186 - x0, 226 - 200)]
    pts = [(x * k, y * k) for x, y in pts]
    d.line(pts, fill=INK + (255,), width=10 * k, joint="curve")
    for p in (pts[0], pts[2]):
        d.ellipse((p[0] - 5 * k, p[1] - 5 * k, p[0] + 5 * k, p[1] + 5 * k), fill=INK + (255,))
    d.line(pts, fill=OK_GREEN + (255,), width=5 * k, joint="curve")
    for p in (pts[0], pts[2]):
        d.ellipse((p[0] - 2.5 * k, p[1] - 2.5 * k, p[0] + 2.5 * k, p[1] + 2.5 * k),
                  fill=OK_GREEN + (255,))
    small = big.resize(layer.size, Image.LANCZOS)
    layer.alpha_composite(small)


def sprite_image(kind):
    """Particula a tamano de pantalla (pixel art 1:1)."""
    if kind in CONFETTI:
        return Image.new("RGBA", (5, 5), CONFETTI[kind] + (255,))
    rows = SPRITES[kind]
    im = Image.new("RGBA", (len(rows[0]), len(rows)), (0, 0, 0, 0))
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in SPRITE_COLS:
                im.putpixel((x, y), SPRITE_COLS[ch] + (255,))
    return im.resize((im.width * 2, im.height * 2), Image.NEAREST)  # pixel art a x2


def antenna_tip(body):
    """Centro de la bola de la antena de un cuerpo, en coordenadas del lienzo."""
    im, x, y = body
    a = im.getchannel("A").load()
    top = next(j for j in range(im.height) if any(a[i, j] for i in range(im.width)))
    xs = [i for j in range(top, top + 4) for i in range(im.width) if a[i, j]]
    return x + sum(xs) / len(xs), y + top + 4


def canvas_size(bodies, syms):
    items = bodies + syms
    return (max(x + im.width for im, x, _ in items), max(y + im.height for im, _, y in items))


def binarize(im):
    """Como lo pintara el firmware (I8 con indice 0 transparente): alfa a 0/255 tras
    mezclar los bordes semitransparentes con el fondo de la UI."""
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    ipx, opx = im.load(), out.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = ipx[x, y]
            if a >= 96:
                t = a / 255
                opx[x, y] = (round(r * t + BG[0] * (1 - t)), round(g * t + BG[1] * (1 - t)),
                             round(b * t + BG[2] * (1 - t)), 255)
    return out


def compose(bodies, syms, size, body, sym, bob=0):
    frame = Image.new("RGBA", size, BG)
    im, x, y = bodies[body]
    frame.alpha_composite(binarize(im), (x, y + bob))
    if sym >= 0:
        s, sx, sy = syms[sym]
        frame.alpha_composite(binarize(s), (sx, sy + bob))
    return frame


def save_preview(tag, bodies, syms):
    size = canvas_size(bodies, syms)
    frames, durs = [], []
    t = 0
    for body, sym, ms in SEQ:
        # respiracion como el bebe de Inicio: 3 px, 2600 ms por sentido (muestreada cada 100 ms)
        left = ms
        while left > 0:
            step = min(100, left)
            phase = (t % 5200) / 2600
            bob = round(3 * (phase if phase < 1 else 2 - phase))
            frames.append(compose(bodies, syms, size, body, sym, -bob))
            durs.append(step)
            t += step
            left -= step
    big = [f.resize((f.width * 2, f.height * 2), Image.NEAREST).convert("RGB") for f in frames]
    big[0].save(os.path.join(PREVIEW, f"nowifi_{tag}.gif"), save_all=True, append_images=big[1:],
                duration=durs, loop=0, disposal=1)
    # hoja: los fotogramas distintos, a tamano real
    uniq = []
    for body, sym, _ in SEQ:
        if (body, sym) not in uniq:
            uniq.append((body, sym))
    sheet = Image.new("RGBA", ((size[0] + 6) * len(uniq) + 6, size[1] + 12), BG)
    for i, (body, sym) in enumerate(uniq):
        sheet.alpha_composite(compose(bodies, syms, size, body, sym), (6 + i * (size[0] + 6), 6))
    sheet.save(os.path.join(PREVIEW, f"nowifi_{tag}_sheet.png"))
    return size


SCREEN_Y_BOTTOM = 249  # el lienzo acaba encima del texto (252..270) y de los puntos de pagina


def mock_screen():
    """Fondo de Inicio para las vistas previas: marca, boton del pais y barra de navegacion."""
    scr = Image.new("RGBA", (240, 320), BG)
    wm = Image.open(os.path.join(PREVIEW, "wordmark.png")).convert("RGBA")
    scr.alpha_composite(wm.resize((wm.width // 2, wm.height // 2), Image.LANCZOS), (8, 8))
    d = ImageDraw.Draw(scr)
    d.rounded_rectangle((176, 6, 234, 24), 8, fill=(255, 255, 255, 255), outline=(154, 161, 170, 255))
    d.text((186, 9), "Ghana", fill=INK + (255,))
    d.rectangle((0, 280, 240, 320), fill=(255, 255, 255, 255))
    d.line((0, 280, 240, 280), fill=(154, 161, 170, 255))
    for i in range(5):
        d.ellipse((24 + i * 48 - 8, 292, 24 + i * 48 + 8, 308), outline=(154, 161, 170, 255), width=2)
    return scr


def particles_at(t_ms, tip):
    """[(sprite, x, y, opa)] de la explosion t_ms despues de empezar (px de pantalla)."""
    import math
    out = []
    for kind, ang, v, delay in PARTICLES:
        t = t_ms - delay
        if t < 0 or t > CELEB_LIFE:
            continue
        ts = t / 1000
        a = math.radians(ang)
        x = tip[0] + v * math.cos(a) * ts
        y = tip[1] + v * math.sin(a) * ts + 0.5 * CELEB_G * ts * ts
        opa = 1.0 if t < CELEB_LIFE - CELEB_FADE else (CELEB_LIFE - t) / CELEB_FADE
        out.append((kind, round(x), round(y), opa))
    return out


def save_screen_previews(tag, bodies, syms):
    """nowifi_pantalla_<tag>.gif (bucle sin WiFi) y nowifi_conecta_<tag>.gif (un poco del
    bucle, la celebracion y el fundido a Inicio), en una pantalla de 240x320 a x2."""
    size = canvas_size(bodies, syms)
    ox, oy = (240 - size[0]) // 2, SCREEN_Y_BOTTOM - size[1]
    base = mock_screen()
    sprites = {k: sprite_image(k) for k in list(SPRITES) + list(CONFETTI)}
    tip = antenna_tip(bodies[3])
    tip = (ox + tip[0], oy + tip[1])

    def shot(body, sym, dy, parts=(), fade=0.0):
        scr = base.copy()
        scr.alpha_composite(compose_clear(bodies, syms, size, body, sym, dy), (ox, oy))
        for kind, x, y, opa in parts:
            sp = sprites[kind].copy()
            sp.putalpha(sp.getchannel("A").point(lambda v, o=opa: round(v * o)))
            scr.alpha_composite(sp, (x - sp.width // 2, y - sp.height // 2))
        if fade:
            scr = Image.blend(scr, base, fade)
        return scr.resize((480, 640), Image.NEAREST).convert("RGB")

    def breathing(seq, t0=0):
        frames, durs, t = [], [], t0
        for body, sym, ms in seq:
            left = ms
            while left > 0:
                step = min(100, left)
                ph = (t % 5200) / 2600
                frames.append(shot(body, sym, -round(3 * (ph if ph < 1 else 2 - ph))))
                durs.append(step)
                t += step
                left -= step
        return frames, durs

    frames, durs = breathing(SEQ)
    frames[0].save(os.path.join(PREVIEW, f"nowifi_pantalla_{tag}.gif"), save_all=True,
                   append_images=frames[1:], duration=durs, loop=0)

    # conexion: los ultimos 1,5 s del bucle, la celebracion y 400 ms de fundido
    frames, durs = breathing([(4, 4, 700), (4, 3, 300), (4, 4, 500)])
    t, burst, hops = 0, None, 0
    prev_body = 4
    for body, sym, ms, dy in CELEB_SEQ:
        if body == 3 and prev_body != 3:
            hops += 1
            if hops == CELEB_BURST:
                burst = t
        prev_body = body
        left = ms
        while left > 0:
            step = min(80, left)
            parts = particles_at(t - burst, tip) if burst is not None else ()
            frames.append(shot(body, sym, dy, parts))
            durs.append(step)
            t += step
            left -= step
    last = CELEB_SEQ[-1]
    for i in range(1, 6):
        frames.append(shot(last[0], last[1], 0, fade=i / 5))
        durs.append(80)
    durs[-1] = 1200
    frames[0].save(os.path.join(PREVIEW, f"nowifi_conecta_{tag}.gif"), save_all=True,
                   append_images=frames[1:], duration=durs, loop=0)


def compose_clear(bodies, syms, size, body, sym, dy):
    """Como compose() pero con fondo transparente (para ponerlo sobre la pantalla)."""
    frame = Image.new("RGBA", size, (0, 0, 0, 0))
    im, x, y = bodies[body]
    frame.alpha_composite(binarize(im), (x, max(0, y + dy)))
    if sym >= 0:
        sm, sx, sy = syms[sym]
        frame.alpha_composite(binarize(sm), (sx, max(0, sy + dy)))
    return frame


def main():
    os.makedirs(PREVIEW, exist_ok=True)
    boy = build(False)
    girl = build(True)
    size = save_preview("boy", *boy)
    save_preview("girl", *girl)
    save_screen_previews("boy", *boy)
    save_screen_previews("girl", *girl)
    print("lienzo:", size, " cuerpos:", [(im.size, x, y) for im, x, y in boy[0]])
    print("simbolo:", [(im.size, x, y) for im, x, y in boy[1]][-1])
    if "--emit" in sys.argv:
        emit(boy, girl, size)


def quantize_all(images):
    """Una paleta comun (255 colores + transparente) para todos los cuerpos."""
    bins = [binarize(im) for im in images]
    W = sum(b.width for b in bins)
    H = max(b.height for b in bins)
    strip = Image.new("RGB", (W, H), BG[:3])
    x = 0
    for b in bins:
        strip.paste(b.convert("RGB"), (x, 0))
        x += b.width
    q = strip.quantize(colors=255, method=Image.MEDIANCUT)
    pal = q.getpalette()[: 255 * 3]
    palette = [(0, 0, 0, 0)] + [(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2], 255) for i in range(255)]
    qpx = q.load()
    out, x = [], 0
    for b in bins:
        bpx = b.load()
        idxs = [0 if bpx[i, j][3] == 0 else qpx[x + i, j] + 1
                for j in range(b.height) for i in range(b.width)]
        out.append((idxs, b.size))
        x += b.width
    return palette, out


def emit(boy, girl, size):
    """img_nowifi.c + assets_nowifi.h. Cuerpos I8 (paleta por sexo); el simbolo y las
    particulas son los mismos para nino y nina."""
    import math
    externs = []
    with open(os.path.join(ASSETS, "img_nowifi.c"), "w", encoding="utf-8", newline="\n") as f:
        f.write(c_header("Bebe sin WiFi (nino y nina): cuerpos I8, simbolo del WiFi por partes y "
                         "particulas de la celebracion"))
        for tag, (bodies, _) in (("boy", boy), ("girl", girl)):
            palette, q = quantize_all([im for im, _, _ in bodies])
            for i, (idxs, sz) in enumerate(q):
                n = f"img_nowifi_{tag}_{i}"
                emit_i8(f, n, palette, idxs, sz)
                externs.append(n)
        for i, (im, _, _) in enumerate(boy[1]):
            n = f"img_nowifi_sym{i}"
            emit_argb8888(f, n, binarize(im))
            externs.append(n)
        for kind in SPRITES:
            n = f"img_nowifi_{kind}"
            emit_argb8888(f, n, sprite_image(kind))
            externs.append(n)

    def pos(items):
        return "{ " + ", ".join(f"{{{x}, {y}}}" for _, x, y in items) + " }"

    kinds = list(SPRITES) + list(CONFETTI)
    tip_b, tip_g = antenna_tip(boy[0][3]), antenna_tip(girl[0][3])
    parts = []
    for kind, ang, v, delay in PARTICLES:
        a = math.radians(ang)
        parts.append(f"{{{kinds.index(kind)}, {round(v * math.cos(a))}, {round(v * math.sin(a))}, {delay}}}")
    conf = ", ".join(f"0x{r:02X}{g:02X}{b:02X}" for r, g, b in CONFETTI.values())
    seq = ", ".join(f"{{{b}, {sy}, {ms}}}" for b, sy, ms in SEQ)
    celeb = ", ".join(f"{{{b}, {sy}, {ms}, {dy}}}" for b, sy, ms, dy in CELEB_SEQ)
    extra = "\n".join([
        "/* lienzo (px de pantalla) */",
        f"#define NOWIFI_W {size[0]}",
        f"#define NOWIFI_H {size[1]}",
        f"#define NOWIFI_BODIES {len(FRAMES)}",
        f"#define NOWIFI_SYMS {len(SYM_STEPS)}",
        "/* esquina de cada cuerpo y de cada paso del simbolo dentro del lienzo */",
        f"#define NOWIFI_BOY_BODY_POS {pos(boy[0])}",
        f"#define NOWIFI_GIRL_BODY_POS {pos(girl[0])}",
        f"#define NOWIFI_SYM_POS {pos(boy[1])}",
        "/* punta de la antena del cuerpo 3 (de donde salen las particulas), en el lienzo */",
        f"#define NOWIFI_BOY_TIP_X {round(tip_b[0])}",
        f"#define NOWIFI_BOY_TIP_Y {round(tip_b[1])}",
        f"#define NOWIFI_GIRL_TIP_X {round(tip_g[0])}",
        f"#define NOWIFI_GIRL_TIP_Y {round(tip_g[1])}",
        "/* bucle sin WiFi: {cuerpo, simbolo (-1 = ninguno), ms} */",
        f"#define NOWIFI_SEQ_LEN {len(SEQ)}",
        f"#define NOWIFI_SEQ {{ {seq} }}",
        "/* asistente: busca senal sin aspa; desde NOWIFI_SEARCH_LOOP se repite */",
        f"#define NOWIFI_SEARCH_LEN {len(SEARCH_SEQ)}",
        f"#define NOWIFI_SEARCH_LOOP {SEARCH_LOOP}",
        "#define NOWIFI_SEARCH { " + ", ".join(f"{{{b}, {sy}, {ms}}}" for b, sy, ms in SEARCH_SEQ) + " }",
        "/* celebracion al conectar: {cuerpo, simbolo, ms, salto px} */",
        f"#define NOWIFI_CELEB_LEN {len(CELEB_SEQ)}",
        f"#define NOWIFI_CELEB {{ {celeb} }}",
        f"#define NOWIFI_CELEB_BURST {CELEB_BURST} /* la explosion sale con este salto (desde 1) */",
        "/* particulas: {tipo, vx, vy (px/s), retraso ms}; tipo 0 corazon, 1 estrella, 2.. confeti */",
        "#define NOWIFI_PART_HEART 0",
        "#define NOWIFI_PART_STAR 1",
        f"#define NOWIFI_N_PARTS {len(PARTICLES)}",
        f"#define NOWIFI_PARTS {{ {', '.join(parts)} }}",
        f"#define NOWIFI_CONFETTI_COLS {{ {conf} }}",
        "#define NOWIFI_CONFETTI_SIZE 5",
        f"#define NOWIFI_G {CELEB_G}",
        f"#define NOWIFI_LIFE_MS {CELEB_LIFE}",
        f"#define NOWIFI_FADE_MS {CELEB_FADE}",
        "",
    ])
    write_header(os.path.join(ASSETS, "include", "assets_nowifi.h"),
                 "Bebe sin WiFi (gen_nowifi.py)", externs, extra)
    print("img_nowifi.c escrito")


if __name__ == "__main__":
    main()
