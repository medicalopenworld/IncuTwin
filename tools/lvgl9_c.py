"""Emisores de imagenes en C para LVGL 9 (lv_image_dsc_t).

Formatos usados por IncuTwin:
  - I8       : 256 entradas de paleta (B,G,R,A) + 1 byte/px. Bebe animado: la paleta se
               sobrescribe en RAM para cambiar el tono de piel.
  - ARGB8888 : 4 bytes/px (B,G,R,A). Iconos, marca, logo, corazon.
  - RGB565   : 2 bytes/px little-endian. Imagenes a pantalla completa (sin alfa).

Compartido por gen_assets.py y gen_extra.py.
"""

PALETTE_ENTRIES = 256


def c_header(comment):
    return (f"/* {comment}\n"
            " * Generado por tools/ — NO editar a mano. Formato lv_image_dsc_t de LVGL 9. */\n"
            '#include "lvgl.h"\n\n')


def _emit_rows(f, rows):
    for row in rows:
        f.write("  " + ", ".join(f"0x{v:02x}" for v in row) + ",\n")


def _emit_dsc(f, name, cf, w, h, stride, data_size):
    f.write(f"const lv_image_dsc_t {name} = {{\n")
    f.write("  .header.magic = LV_IMAGE_HEADER_MAGIC,\n")
    f.write(f"  .header.cf = {cf},\n")
    f.write("  .header.flags = 0,\n")
    f.write(f"  .header.w = {w},\n  .header.h = {h},\n  .header.stride = {stride},\n")
    f.write(f"  .data_size = {data_size},\n  .data = {name}_map,\n}};\n\n")


def emit_i8(f, name, palette, idxs, size):
    """palette: lista de (r,g,b,a) de hasta 256 entradas; idxs: w*h indices."""
    w, h = size
    f.write(f"static const uint8_t {name}_map[] = {{\n  /* paleta: {PALETTE_ENTRIES} x (B,G,R,A) */\n")
    pal_rows = []
    for i in range(PALETTE_ENTRIES):
        r, g, b, a = palette[i] if i < len(palette) else (0, 0, 0, 0)
        pal_rows.append((b, g, r, a))
    _emit_rows(f, pal_rows)
    f.write("  /* indices */\n")
    _emit_rows(f, (idxs[y * w:(y + 1) * w] for y in range(h)))
    f.write("};\n\n")
    _emit_dsc(f, name, "LV_COLOR_FORMAT_I8", w, h, w, PALETTE_ENTRIES * 4 + w * h)


def emit_argb8888(f, name, im):
    w, h = im.size
    px = im.convert("RGBA").load()
    f.write(f"static const uint8_t {name}_map[] = {{\n")
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            r, g, b, a = px[x, y]
            row += [b, g, r, a]
        rows.append(row)
    _emit_rows(f, rows)
    f.write("};\n\n")
    _emit_dsc(f, name, "LV_COLOR_FORMAT_ARGB8888", w, h, w * 4, w * h * 4)


def emit_rgb565(f, name, im):
    w, h = im.size
    px = im.convert("RGB").load()
    f.write(f"static const uint8_t {name}_map[] = {{\n")
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            r, g, b = px[x, y]
            c = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            row += [c & 0xFF, c >> 8]
        rows.append(row)
    _emit_rows(f, rows)
    f.write("};\n\n")
    _emit_dsc(f, name, "LV_COLOR_FORMAT_RGB565", w, h, w * 2, w * h * 2)


def emit_palettes(f, name, pals):
    f.write(f"const uint8_t {name}[{len(pals)}][{PALETTE_ENTRIES} * 4] = {{\n")
    for p in pals:
        f.write("  {\n")
        for i in range(PALETTE_ENTRIES):
            r, g, b, a = p[i] if i < len(p) else (0, 0, 0, 0)
            f.write(f"    0x{b:02x}, 0x{g:02x}, 0x{r:02x}, 0x{a:02x},\n")
        f.write("  },\n")
    f.write("};\n\n")


def write_header(path, guard_comment, externs, extra=""):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(f"/* {guard_comment}\n * Generado por tools/ — NO editar a mano. */\n")
        f.write('#pragma once\n#include "lvgl.h"\n\n#ifdef __cplusplus\nextern "C" {\n#endif\n\n')
        for e in externs:
            f.write(f"extern const lv_image_dsc_t {e};\n")
        if extra:
            f.write("\n" + extra)
        f.write("\n#ifdef __cplusplus\n}\n#endif\n")
