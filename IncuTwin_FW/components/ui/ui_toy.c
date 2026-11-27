/* Piezas del estilo «IncuTwin de bolsillo» (ui_toy.h). */
#include "ui_toy.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "fonts.h"
#include "ui_theme.h"

static const char *TAG = "toy";

/* ------------------------------------------------------------- sombra dura */

/* LVGL no pinta sombras sin difuminar (shadow_width 0 = sin sombra): se dibuja un bloque tinta
 * antes de la pieza, en su zona extra de dibujo. Pulsada, la pieza baja TOY_SINK (estilo translate
 * del estado PRESSED) y el bloque se dibuja TOY_SINK mas cerca: la sombra no se mueve. */
static void shadow_cb(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target_obj(e);
    int32_t px = (int32_t)(intptr_t)lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_event_set_ext_draw_size(e, px);
        return;
    }
    int32_t off = px;
    if (lv_obj_has_state(o, LV_STATE_PRESSED) && lv_obj_is_clickable(o)) off -= TOY_SINK;
    if (off <= 0) return;
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = COL_INK;
    d.bg_opa = LV_OPA_COVER;
    d.radius = lv_obj_get_style_radius(o, LV_PART_MAIN);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    lv_area_move(&a, off, off);
    lv_draw_rect(lv_event_get_layer(e), &d, &a);
}

void toy_shadow(lv_obj_t *o, int32_t px)
{
    while (lv_obj_remove_event_cb(o, shadow_cb)) {
    }
    lv_obj_set_style_shadow_width(o, 0, 0); /* nada de la sombra difuminada del tema */
    if (px > 0) {
        lv_obj_add_event_cb(o, shadow_cb, LV_EVENT_DRAW_MAIN_BEGIN, (void *)(intptr_t)px);
        lv_obj_add_event_cb(o, shadow_cb, LV_EVENT_REFR_EXT_DRAW_SIZE, (void *)(intptr_t)px);
        int32_t sink = px < TOY_SINK ? px : TOY_SINK;
        lv_obj_set_style_translate_x(o, sink, LV_STATE_PRESSED);
        lv_obj_set_style_translate_y(o, sink, LV_STATE_PRESSED);
    } else {
        lv_obj_set_style_translate_x(o, 0, LV_STATE_PRESSED);
        lv_obj_set_style_translate_y(o, 0, LV_STATE_PRESSED);
    }
    lv_obj_refresh_ext_draw_size(o);
    lv_obj_invalidate(o);
}

/* ------------------------------------------------------------- piezas */

void toy_box(lv_obj_t *o, int32_t radius, int32_t shadow)
{
    lv_obj_set_style_bg_color(o, COL_CARD, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(o, COL_INK, 0);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, TOY_BORDER, 0);
    lv_obj_set_style_border_side(o, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_outline_width(o, 0, LV_STATE_FOCUS_KEY);
    toy_shadow(o, shadow);
}

static void label_font(lv_obj_t *parent, const lv_font_t *font, lv_color_t col)
{
    uint32_t n = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(parent, (int32_t)i);
        if (!lv_obj_check_type(c, &lv_label_class)) continue;
        lv_obj_set_style_text_font(c, font, 0);
        lv_obj_set_style_text_color(c, col, 0);
    }
}

void toy_button(lv_obj_t *b, bool primary)
{
    toy_box(b, TOY_R_BTN, TOY_SHADOW_M);
    lv_obj_set_style_bg_color(b, primary ? COL_CORAL : COL_CARD, 0);
    /* sin el oscurecido del tema al pulsar (recolor negro al 35 %): responde hundiendose */
    lv_obj_set_style_bg_color(b, primary ? COL_CORAL : COL_CARD, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_recolor_opa(b, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_text_font(b, &lv_font_title_14, 0);
    lv_obj_set_style_text_color(b, primary ? lv_color_white() : COL_INK, 0);
    label_font(b, &lv_font_title_14, primary ? lv_color_white() : COL_INK);
}

void toy_pill(lv_obj_t *o)
{
    toy_box(o, TOY_R_PILL, TOY_SHADOW_S);
}

void toy_segment(lv_obj_t *b, lv_obj_t *lbl, bool on)
{
    toy_box(b, 8, TOY_SHADOW_S);
    lv_obj_set_style_bg_color(b, on ? COL_INK : COL_CARD, 0);
    lv_obj_set_style_bg_color(b, on ? COL_INK : COL_CARD, LV_STATE_PRESSED);
    lv_obj_set_style_recolor_opa(b, LV_OPA_TRANSP, LV_STATE_PRESSED);
    if (lbl) lv_obj_set_style_text_color(lbl, on ? lv_color_white() : COL_INK, 0);
}

/* ------------------------------------------------------------ corazones */

/* Cuadricula de la app (apps/mobile/src/components/toy/heartGrid.ts), de arriba abajo */
static const char *const GRID[8] = { "011000110", "111101111", "111111111", "111111111",
                                     "011111110", "001111100", "000111000", "000010000" };

static bool cell_on(int x, int y) { return x >= 0 && x < 9 && y >= 0 && y < 8 && GRID[y][x] == '1'; }

/* Contorno: celda encendida que toca una apagada en horizontal o vertical */
static bool cell_edge(int x, int y)
{
    return !cell_on(x - 1, y) || !cell_on(x + 1, y) || !cell_on(x, y - 1) || !cell_on(x, y + 1);
}

typedef struct {
    toy_heart_t kind;
    int32_t cell;
    lv_image_dsc_t dsc;
} heart_cache_t;

#define HEART_CACHE_N 24
static heart_cache_t s_cache[HEART_CACHE_N];
static int s_cache_n;

static void heart_cols(toy_heart_t k, lv_color_t *fill, lv_color_t *edge)
{
    switch (k) {
    case TOY_HEART_EMPTY: *fill = COL_CARD; *edge = COL_FAINT; break;
    case TOY_HEART_TODAY: *fill = COL_CARD; *edge = COL_CORAL; break;
    case TOY_HEART_SUPER: *fill = *edge = COL_SUPER; break;
    case TOY_HEART_REST:  *fill = *edge = COL_REST; break;
    case TOY_HEART_WHITE: *fill = *edge = lv_color_white(); break;
    default:              *fill = *edge = COL_CORAL; break;
    }
}

const lv_image_dsc_t *toy_heart_dsc(toy_heart_t kind, int32_t cell)
{
    if (cell < 1) cell = 1;
    if ((int)kind >= TOY_HEART_COUNT) kind = TOY_HEART_FULL;
    for (int i = 0; i < s_cache_n; i++) {
        if (s_cache[i].kind == kind && s_cache[i].cell == cell) return &s_cache[i].dsc;
    }
    if (s_cache_n == HEART_CACHE_N) { /* no deberia pasar: hoy se usan unos 11 */
        ESP_LOGW(TAG, "cache de corazones llena (%d)", HEART_CACHE_N);
        return &s_cache[0].dsc;
    }
    int32_t w = 9 * cell, h = 8 * cell;
    size_t bytes = (size_t)w * (size_t)h * 4u;
    /* en PSRAM: lo pequeño (< 16 KB) iria a la RAM interna, la que necesitan WiFi y TLS */
    uint32_t *px = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!px) px = lv_malloc(bytes);
    if (!px) return NULL;
    lv_color_t fill, edge;
    heart_cols(kind, &fill, &edge);
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            int cx = (int)(x / cell), cy = (int)(y / cell);
            uint32_t v = 0; /* transparente */
            if (cell_on(cx, cy)) {
                lv_color_t c = cell_edge(cx, cy) ? edge : fill;
                v = 0xFF000000u | ((uint32_t)c.red << 16) | ((uint32_t)c.green << 8) | c.blue;
            }
            px[y * w + x] = v;
        }
    }
    heart_cache_t *hc = &s_cache[s_cache_n++];
    memset(hc, 0, sizeof(*hc));
    hc->kind = kind;
    hc->cell = cell;
    hc->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    hc->dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    hc->dsc.header.w = (uint32_t)w;
    hc->dsc.header.h = (uint32_t)h;
    hc->dsc.header.stride = (uint32_t)w * 4u;
    hc->dsc.data_size = (uint32_t)(w * h * 4);
    hc->dsc.data = (const uint8_t *)px;
    return &hc->dsc;
}

lv_obj_t *toy_heart_img(lv_obj_t *parent, toy_heart_t kind, int32_t cell)
{
    lv_obj_t *i = lv_image_create(parent);
    lv_image_set_antialias(i, false); /* al latir (escala) que los pixeles sigan duros */
    lv_image_set_src(i, toy_heart_dsc(kind, cell));
    lv_obj_set_clickable(i, false);
    return i;
}
