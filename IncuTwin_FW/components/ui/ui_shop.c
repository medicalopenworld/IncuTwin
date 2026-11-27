/* Tienda (cambio openspec `tienda`, spec twin-display "Pantalla Tienda"): se abre desde Corazones
 * y tapa la lista hasta la barra de navegacion (que sigue con Corazones activo).
 *   arriba:   «‹», «Tienda» y el saldo
 *   pestañas: Gorros (con Niño / Niña), Mantas, Fondos y Efectos
 *   tarjetas: muestra, nombre y estado (Elegido, Usar, precio, Gratis, Desde el 24, 3/10, Pedida)
 *   Probar:   gorros, mantas y Papa Noel se ven puestos en un bebe propio antes de gastar
 * Todo lo que cambia va por `rewards` (rewards_unlock) y vuelve como TWIN_EVT_REWARDS_CHANGED:
 * la tienda se rehace en el refresco de ui.c, nunca dentro del evento de un toque. */
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "baby_widget.h"
#include "rewards.h"
#include "ui_internal.h"
#include "ui_toy.h"

#define TOP_H    40
#define TABS_Y   42
#define SUB_Y    70
#define GRID_Y   96
#define CARD_W   70  /* tres por fila con hueco para la sombra dura: 9 + 3x70 + 2x6 + 2 */
#define CARD_H   90
#define WELL_W   62
#define WELL_H   48
#define NAME_Y   (WELL_H + 5)
#define STATUS_Y (NAME_Y + 16)
#define TAB_W    53 /* cuatro pestañas de ancho fijo: (240 - 2*6 - 3*5) / 4 */
#define COL_WELL lv_color_hex(0xEEF4FB)
#define COL_XMAS lv_color_hex(0x2E7D5B)
#define COL_SNOW lv_color_hex(0x9EC2E6)

/* «Una IncuNest con tu nombre» no esta aqui: tiene su tarjeta en Corazones (ui_shop_special_clicked) */
typedef enum { TAB_HATS = 0, TAB_BLANKETS, TAB_BGS, TAB_FX, TAB_N } shop_tab_t;

static lv_obj_t *s_root, *s_title, *s_bal, *s_tab_btn[TAB_N], *s_tab_lbl[TAB_N], *s_sub, *s_seg, *s_seg_btn[2];
static lv_obj_t *s_grid, *s_try;
static shop_tab_t s_tab;
static bool s_girl;                       /* Gorros: pestaña Niña */
static rw_item_t s_try_item = RW_ITEM_COUNT; /* lo que se esta probando; COUNT = rejilla */
static rw_item_t s_pending;               /* dialogo de compra abierto */

/* ------------------------------------------------------------- catalogo */

static int tab_items(shop_tab_t t, rw_item_t *out)
{
    int n = 0;
    switch (t) {
    case TAB_HATS:
        for (int i = s_girl ? RW_HAT_GIRL_FIRST : RW_HAT_BOY_FIRST; i <= (s_girl ? RW_HAT_GIRL_LAST : RW_HAT_BOY_LAST); i++) {
            out[n++] = (rw_item_t)i;
        }
        out[n++] = RW_ITEM_SANTA;
        break;
    case TAB_BLANKETS:
        for (int i = RW_BLANKET_FIRST; i <= RW_BLANKET_LAST; i++) out[n++] = (rw_item_t)i;
        break;
    case TAB_BGS:
        for (int i = RW_BG_FIRST; i <= RW_BG_LAST; i++) out[n++] = (rw_item_t)i;
        out[n++] = RW_ITEM_GARLAND;
        break;
    case TAB_FX:
        out[n++] = RW_ITEM_SNOW;
        for (int i = RW_HALO_FIRST; i <= RW_HALO_LAST; i++) out[n++] = (rw_item_t)i;
        break;
    default:
        break;
    }
    return n;
}

static bool is_hat(rw_item_t i)
{
    return (i >= RW_HAT_BOY_FIRST && i <= RW_HAT_BOY_LAST) || (i >= RW_HAT_GIRL_FIRST && i <= RW_HAT_GIRL_LAST);
}
static bool is_blanket(rw_item_t i) { return i >= RW_BLANKET_FIRST && i <= RW_BLANKET_LAST; }
static bool triable(rw_item_t i) { return is_hat(i) || is_blanket(i) || i == RW_ITEM_SANTA; }
static bool owns(const rw_state_t *r, rw_item_t i) { return r->unlocked & RW_BIT(i); }

static bool chosen(const rw_state_t *r, rw_item_t i)
{
    if (!owns(r, i)) return false;
    switch (i) {
    case RW_ITEM_SANTA:          return r->santa_on;
    case RW_ITEM_SNOW:           return r->snow_on;
    case RW_ITEM_GARLAND:        return r->garland_on;
    case RW_ITEM_NAMED_INCUNEST: return false;
    default:
        return r->halo == i || r->bg == i || r->hat_boy == i || r->hat_girl == i || r->blanket == i;
    }
}

static bool off_season(rw_item_t i)
{
    int m, d;
    rewards_local_md(&m, &d);
    return rw_is_xmas(i) && !rw_xmas_season(m, d);
}

/* ------------------------------------------------------------- piezas */

static lv_obj_t *box(lv_obj_t *parent)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_radius(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_set_scrollable(b, false);
    lv_obj_set_clickable(b, false);
    return b;
}

static lv_obj_t *label(lv_obj_t *parent, const char *t, const lv_font_t *f, lv_color_t col)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, col, 0);
    return l;
}

/* Corazon de pixeles de la app (9x8 celdas de `cell` px) */
static lv_obj_t *heart_img(lv_obj_t *parent, toy_heart_t kind, int32_t cell)
{
    return toy_heart_img(parent, kind, cell);
}

/* Un bebe con la ropa de `item` puesta (y lo elegido en lo demas) */
static lv_obj_t *dressed_baby(lv_obj_t *parent, rw_item_t item, bool girl, uint16_t scale)
{
    const rw_state_t *r = ui_rewards();
    rw_item_t hat = (rw_item_t)(girl ? r->hat_girl : r->hat_boy);
    if (is_hat(item)) hat = item;
    rw_item_t blanket = is_blanket(item) ? item : (rw_item_t)r->blanket;
    lv_obj_t *b = baby_preview_create(parent, girl, baby_get_skin_tone(), ui_hat_style(hat), ui_blanket_style(blanket),
                                      item == RW_ITEM_SANTA, scale);
    return b;
}

static void well_art(lv_obj_t *well, rw_item_t item)
{
    if (is_hat(item) || item == RW_ITEM_SANTA) { /* la cabeza, a media escala */
        lv_obj_t *b = dressed_baby(well, item, item == RW_ITEM_SANTA ? s_girl : item >= RW_HAT_GIRL_FIRST, 128);
        lv_obj_set_pos(b, (WELL_W - 57) / 2 - (item == RW_ITEM_SANTA ? 4 : 0), item == RW_ITEM_SANTA ? 11 : 3);
    } else if (is_blanket(item)) { /* el bebe entero, pequeño */
        lv_obj_t *b = dressed_baby(well, item, s_girl, 64); /* 28 x 46 */
        lv_obj_set_pos(b, (WELL_W - 28) / 2, 1);
    } else if (item == RW_ITEM_GARLAND) {
        lv_obj_t *g = lv_image_create(well);
        lv_image_set_src(g, &img_garland);
        lv_obj_set_pos(g, -63, 13);
    } else if (item == RW_ITEM_NAMED_INCUNEST) {
        lv_obj_t *n = lv_image_create(well);
        lv_image_set_src(n, &img_incunest_28);
        lv_obj_center(n);
    } else if (item == RW_ITEM_SNOW) {
        lv_obj_set_style_bg_color(well, lv_color_hex(0x2C385C), 0);
        static const int8_t XY[][2] = { { 8, 4 }, { 44, 8 }, { 24, 20 }, { 50, 28 }, { 10, 30 } };
        for (unsigned k = 0; k < sizeof(XY) / sizeof(XY[0]); k++) {
            lv_obj_t *f = label(well, "*", &lv_font_es_20, lv_color_white());
            lv_obj_set_pos(f, XY[k][0], XY[k][1]);
        }
    } else { /* halo o fondo: el color */
        bool halo = item <= RW_HALO_LAST;
        lv_obj_t *sw = box(well);
        lv_obj_set_size(sw, halo ? 38 : WELL_W, halo ? 38 : WELL_H);
        lv_obj_set_style_radius(sw, halo ? LV_RADIUS_CIRCLE : 6, 0);
        lv_obj_set_style_bg_color(sw, ui_item_color(item), 0);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
        if (!halo) {
            lv_obj_set_size(sw, WELL_W - 14, WELL_H - 12);
            lv_obj_set_style_border_width(sw, TOY_BORDER, 0);
            lv_obj_set_style_border_color(sw, COL_INK, 0);
        }
        lv_obj_center(sw);
    }
}

/* Estado de la tarjeta, debajo del nombre: si no cabe («Desde el 24»), pasa despacio */
static lv_obj_t *status_label(lv_obj_t *card, const char *t, lv_color_t col)
{
    lv_obj_t *l = label(card, t, &lv_font_es_12, col);
    lv_obj_set_width(l, CARD_W - 2 * TOY_BORDER - 6);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_pos(l, 4, STATUS_Y);
    return l;
}

static void card_status(lv_obj_t *card, const rw_state_t *r, rw_item_t item)
{
    char buf[24];
    if (off_season(item)) {
        status_label(card, tr(STR_SHOP_FROM24), COL_XMAS);
        return;
    }
    if (item == RW_ITEM_NAMED_INCUNEST && owns(r, item)) {
        lv_obj_set_pos(label(card, tr(STR_REDEEMED), &lv_font_es_12, COL_OK), 4, STATUS_Y);
        return;
    }
    if (owns(r, item)) {
        bool on = chosen(r, item);
        snprintf(buf, sizeof(buf), on ? LV_SYMBOL_OK " %s" : "%s", tr(on ? STR_CUST_CHOSEN : STR_CUST_USE));
        lv_obj_set_pos(label(card, buf, &lv_font_es_12, on ? COL_CORAL_INK : COL_MUTED), 4, STATUS_Y);
        return;
    }
    if (item == RW_ITEM_GARLAND) { /* lo que llevas de diciembre */
        int m, d;
        unsigned days = rewards_local_md(&m, &d) && m == 12 ? (unsigned)__builtin_popcount(r->dec_days) : 0;
        snprintf(buf, sizeof(buf), "%u/10", days);
        lv_obj_set_pos(label(card, buf, &lv_font_es_12, COL_MUTED), 4, STATUS_Y);
        return;
    }
    uint16_t price = rw_price(item);
    lv_obj_t *row = box(card);
    lv_obj_set_size(row, LV_SIZE_CONTENT, 17);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 3, 0);
    lv_obj_set_pos(row, 4, STATUS_Y);
    heart_img(row, TOY_HEART_FULL, 2); /* 18x16 */
    snprintf(buf, sizeof(buf), "%u", (unsigned)price);
    label(row, buf, &lv_font_es_12, r->balance >= price ? COL_CORAL_INK : COL_MUTED);
}

/* ------------------------------------------------------------- comprar */

static void close_mb(lv_event_t *e) { lv_msgbox_close(lv_event_get_user_data(e)); }

static void buy_confirmed(lv_event_t *e)
{
    lv_msgbox_close(lv_event_get_user_data(e));
    uint16_t missing = 0;
    rewards_unlock(s_pending, &missing);
}

/* Tocar algo para comprarlo o elegirlo (lo que no se prueba, y el boton de Probar) */
static void act_on(rw_item_t item)
{
    const rw_state_t *r = ui_rewards();
    char txt[160];
    if (off_season(item)) {
        ui_msgbox(ui_item_name(item), tr(STR_SHOP_XMAS), tr(STR_OK), COL_NAVY, close_mb, NULL);
        return;
    }
    if (item == RW_ITEM_NAMED_INCUNEST && owns(r, item)) return; /* ya pedida */
    if (owns(r, item)) {
        uint16_t missing;
        rewards_unlock(item, &missing); /* elegir (Navidad: poner / quitar) */
        return;
    }
    if (item == RW_ITEM_GARLAND) {
        ui_msgbox(ui_item_name(item), tr(STR_CUST_GARLAND_HINT), tr(STR_OK), COL_NAVY, close_mb, NULL);
        return;
    }
    bool demo = false;
    rw_state_t tmp;
    rewards_copy(&tmp, &demo);
    if (item == RW_ITEM_NAMED_INCUNEST && demo) {
        ui_msgbox(ui_item_name(item), tr(STR_REDEEM_DEMO), tr(STR_OK), COL_NAVY, close_mb, NULL);
        return;
    }
    uint16_t price = rw_price(item);
    if (r->balance < price) {
        snprintf(txt, sizeof(txt), tr(STR_CUST_MISSING), (unsigned)(price - r->balance));
        ui_msgbox(ui_item_name(item), txt, tr(STR_OK), COL_NAVY, close_mb, NULL);
        return;
    }
    s_pending = item;
    if (item == RW_ITEM_NAMED_INCUNEST) {
        snprintf(txt, sizeof(txt), tr(STR_REDEEM_Q), (unsigned)price);
        ui_msgbox(ui_item_name(item), txt, tr(STR_REDEEM), COL_CORAL, buy_confirmed, NULL);
    } else {
        snprintf(txt, sizeof(txt), tr(STR_SHOP_BUY_Q), (unsigned)price, ui_item_name(item));
        ui_msgbox(ui_item_name(item), txt, tr(STR_SHOP_BUY_OK), COL_CORAL, buy_confirmed, NULL);
    }
}

/* ------------------------------------------------------------- Probar */

static void try_btn_clicked(lv_event_t *e)
{
    (void)e;
    if (s_try_item < RW_ITEM_COUNT) act_on(s_try_item);
}

static void try_build(void)
{
    lv_obj_clean(s_try);
    rw_item_t item = s_try_item;
    const rw_state_t *r = ui_rewards();
    bool girl = is_hat(item) ? item >= RW_HAT_GIRL_FIRST : s_girl;

    /* el bebe dentro de su pantallita, con el fondo elegido, como en Inicio */
    lv_obj_t *stage = box(s_try);
    lv_obj_set_size(stage, 212, 184);
    lv_obj_set_pos(stage, 14, 0);
    lv_obj_set_style_bg_color(stage, ui_theme_bg(), 0);
    lv_obj_set_style_bg_opa(stage, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(stage, COL_INK, 0);
    lv_obj_set_style_border_width(stage, TOY_BORDER, 0);
    lv_obj_set_style_border_post(stage, true, 0); /* el marco por encima del bebe */
    lv_obj_set_style_radius(stage, 8, 0);
    toy_shadow(stage, TOY_SHADOW_M);
    lv_obj_t *halo = box(stage);
    lv_obj_set_size(halo, 166, 166);
    lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(halo, ui_theme_halo_calm(), 0);
    lv_obj_set_style_bg_opa(halo, LV_OPA_40, 0);
    lv_obj_set_pos(halo, (208 - 166) / 2, 7);
    lv_obj_t *b = dressed_baby(stage, item, girl, LV_SCALE_NONE);
    lv_obj_set_pos(b, (208 - 114) / 2, item == RW_ITEM_SANTA ? 20 : -4); /* el sprite trae 3 px vacios arriba */

    ui_str_t hint = item == RW_ITEM_SANTA ? STR_SHOP_XMAS
                    : is_blanket(item)    ? STR_SHOP_WEARS_ALL
                    : girl                ? STR_SHOP_WEARS_GIRL
                                          : STR_SHOP_WEARS_BOY;
    lv_obj_t *h = label(s_try, tr(hint), &lv_font_es_12, COL_MUTED);
    lv_obj_set_width(h, 224);
    lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(h, 8, 191);

    char txt[48];
    bool enabled = true, filled = true, greyed = false; /* greyed: aun no se puede (gris) */
    uint16_t price = rw_price(item);
    if (off_season(item)) {
        snprintf(txt, sizeof(txt), "%s", tr(STR_SHOP_FROM24_LONG));
        enabled = false;
        greyed = true;
    } else if (owns(r, item)) {
        bool on = chosen(r, item);
        if (item == RW_ITEM_SANTA) snprintf(txt, sizeof(txt), "%s", tr(on ? STR_SHOP_TAKE_OFF : STR_SHOP_PUT_ON));
        else snprintf(txt, sizeof(txt), on ? LV_SYMBOL_OK " %s" : "%s", tr(on ? STR_CUST_CHOSEN : STR_CUST_USE));
        enabled = !on || item == RW_ITEM_SANTA;
        filled = false;
    } else if (r->balance >= price) {
        snprintf(txt, sizeof(txt), tr(STR_SHOP_BUY), (unsigned)price);
    } else {
        snprintf(txt, sizeof(txt), tr(STR_CUST_MISSING), (unsigned)(price - r->balance));
        enabled = false;
        greyed = true;
    }
    lv_obj_t *btn = lv_button_create(s_try);
    lv_obj_set_size(btn, 190, 32);
    lv_obj_set_pos(btn, 24, 208);
    /* boton de juguete: coral para comprar, crema para usar; lo que aun no se puede, apagado */
    toy_button(btn, filled && !greyed);
    if (greyed) {
        lv_obj_set_style_border_color(btn, COL_FAINT, 0);
        toy_shadow(btn, 0);
    }
    lv_obj_t *row = box(btn);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_center(row);
    label(row, txt, &lv_font_title_16, greyed ? COL_MUTED : filled ? lv_color_white() : COL_INK);
    if (enabled && filled && price > 0) heart_img(row, TOY_HEART_WHITE, 2);
    if (enabled) {
        lv_obj_add_flag(btn, UI_FLAG_CLICK_SOUND);
        lv_obj_add_event_cb(btn, try_btn_clicked, LV_EVENT_CLICKED, NULL);
    } else {
        lv_obj_set_clickable(btn, false);
    }
}

/* ------------------------------------------------------------- rejilla */

static void refresh_async(void *arg)
{
    (void)arg;
    ui_shop_refresh();
}

static void card_clicked(lv_event_t *e)
{
    rw_item_t item = (rw_item_t)(intptr_t)lv_event_get_user_data(e);
    if (triable(item)) {
        s_try_item = item;
        lv_async_call(refresh_async, NULL); /* la tarjeta tocada se borra al rehacer */
    } else {
        act_on(item);
    }
}

static void grid_build(void)
{
    const rw_state_t *r = ui_rewards();
    int32_t sy = lv_obj_get_scroll_y(s_grid);
    lv_obj_clean(s_grid);
    rw_item_t items[12];
    int n = tab_items(s_tab, items);
    for (int k = 0; k < n; k++) {
        rw_item_t item = items[k];
        bool on = chosen(r, item) && !off_season(item);
        lv_obj_t *c = lv_obj_create(s_grid);
        lv_obj_set_size(c, CARD_W, CARD_H);
        toy_box(c, TOY_R_PILL, TOY_SHADOW_S); /* ficha de juguete; la elegida, con borde coral */
        if (on) lv_obj_set_style_border_color(c, COL_CORAL, 0);
        lv_obj_set_style_pad_all(c, 0, 0);
        lv_obj_set_scrollable(c, false);
        lv_obj_add_flag(c, UI_FLAG_CLICK_SOUND);
        lv_obj_add_event_cb(c, card_clicked, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)item);

        lv_obj_t *well = box(c);
        lv_obj_set_size(well, WELL_W, WELL_H);
        lv_obj_set_pos(well, (CARD_W - 2 * TOY_BORDER - WELL_W) / 2, 2);
        lv_obj_set_style_radius(well, 6, 0);
        lv_obj_set_style_bg_color(well, COL_WELL, 0);
        lv_obj_set_style_bg_opa(well, LV_OPA_COVER, 0);
        lv_obj_set_style_clip_corner(well, true, 0);
        well_art(well, item);

        lv_obj_t *name = label(c, ui_item_name(item), &lv_font_title_12, COL_INK);
        lv_obj_set_width(name, CARD_W - 2 * TOY_BORDER - 6);
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR); /* «Blanco roto» */
        lv_obj_set_pos(name, 4, NAME_Y);
        card_status(c, r, item);
    }
    lv_obj_update_layout(s_grid);
    lv_obj_scroll_to_y(s_grid, sy, LV_ANIM_OFF);
}

/* ------------------------------------------------------------- arriba */

static void tabs_apply(void)
{
    static const ui_str_t NAMES[TAB_N] = { STR_SHOP_HATS, STR_SHOP_BLANKETS, STR_SHOP_BGS, STR_SHOP_FX };
    for (int t = 0; t < TAB_N; t++) {
        bool on = t == (int)s_tab;
        lv_label_set_text(s_tab_lbl[t], tr(NAMES[t]));
        toy_segment(s_tab_btn[t], s_tab_lbl[t], on);
    }
    bool hats = s_tab == TAB_HATS;
    lv_obj_set_hidden(s_seg, !hats);
    lv_obj_set_hidden(s_sub, hats);
    for (int k = 0; k < 2; k++) {
        bool on = (k == 1) == s_girl;
        lv_obj_t *l = lv_obj_get_child(s_seg_btn[k], 0);
        toy_segment(s_seg_btn[k], l, on);
        lv_label_set_text(l, tr(k ? STR_SHOP_GIRL : STR_SHOP_BOY));
    }
    lv_label_set_text(s_sub, s_tab == TAB_BLANKETS ? tr(STR_SHOP_WEARS_ALL) : "");
}

static void tab_clicked(lv_event_t *e)
{
    s_tab = (shop_tab_t)(intptr_t)lv_event_get_user_data(e);
    lv_obj_scroll_to_y(s_grid, 0, LV_ANIM_OFF);
    ui_shop_refresh();
}

static void seg_clicked(lv_event_t *e)
{
    s_girl = (bool)(intptr_t)lv_event_get_user_data(e);
    ui_shop_refresh();
}

static void back_clicked(lv_event_t *e)
{
    (void)e;
    if (s_try_item < RW_ITEM_COUNT) {
        s_try_item = RW_ITEM_COUNT;
        ui_shop_refresh();
    } else {
        lv_obj_set_hidden(s_root, true);
        lv_obj_clean(s_grid); /* suelta los bebes de las tarjetas (PSRAM) */
        ui_tel_screen(UI_TAB_HEARTS); /* vuelve a Corazones */
    }
}

static lv_obj_t *chip(lv_obj_t *parent, lv_event_cb_t cb, intptr_t arg, lv_obj_t **lbl)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, LV_SIZE_CONTENT, 22);
    toy_segment(b, NULL, false);
    lv_obj_set_style_pad_hor(b, 9, 0);
    lv_obj_set_style_pad_ver(b, 0, 0);
    lv_obj_add_flag(b, UI_FLAG_CLICK_SOUND);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)arg);
    *lbl = label(b, "", &lv_font_es_12, COL_INK);
    lv_obj_center(*lbl);
    return b;
}

lv_obj_t *ui_shop_create(void)
{
    /* encima de la pantalla Corazones, hasta la barra de navegacion */
    lv_obj_t *parent = ui_hearts_screen();
    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, 240, UI_NAV_Y);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_radius(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_set_scrollable(s_root, false);

    lv_obj_t *back = lv_button_create(s_root);
    lv_obj_set_size(back, 30, 30); /* atras: chevron en un circulo de juguete, como la app */
    lv_obj_set_pos(back, 7, 5);
    toy_box(back, LV_RADIUS_CIRCLE, TOY_SHADOW_S);
    lv_obj_set_style_pad_all(back, 0, 0);
    lv_obj_set_ext_click_area(back, 8);
    lv_obj_add_flag(back, UI_FLAG_CLICK_SOUND);
    lv_obj_add_event_cb(back, back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_center(label(back, LV_SYMBOL_LEFT, &lv_font_es_14, COL_INK));

    s_title = label(s_root, "", &lv_font_title_20, COL_INK); /* centrado, como la app */
    lv_obj_set_width(s_title, 128);
    lv_obj_set_style_text_align(s_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(s_title, 56, 9);

    lv_obj_t *bal = box(s_root);
    lv_obj_set_size(bal, LV_SIZE_CONTENT, 24);
    lv_obj_set_flex_flow(bal, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(bal, 4, 0);
    lv_obj_set_flex_align(bal, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(bal, LV_ALIGN_TOP_RIGHT, -8, 8);
    heart_img(bal, TOY_HEART_FULL, 2);
    s_bal = label(bal, "", &lv_font_title_16, COL_INK);

    lv_obj_t *tabs = box(s_root); /* se desliza de lado si no caben */
    lv_obj_set_size(tabs, 240, 24);
    lv_obj_set_pos(tabs, 0, TABS_Y);
    lv_obj_set_flex_flow(tabs, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_hor(tabs, 6, 0);
    lv_obj_set_style_pad_column(tabs, 5, 0);
    for (int t = 0; t < TAB_N; t++) {
        s_tab_btn[t] = chip(tabs, tab_clicked, t, &s_tab_lbl[t]);
        lv_obj_set_width(s_tab_btn[t], TAB_W);
        lv_obj_set_style_pad_hor(s_tab_btn[t], 0, 0);
    }

    s_seg = box(s_root);
    lv_obj_set_size(s_seg, 150, 25);
    lv_obj_set_pos(s_seg, 45, SUB_Y);
    lv_obj_set_flex_flow(s_seg, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s_seg, 6, 0);
    for (int k = 0; k < 2; k++) {
        lv_obj_t *l;
        s_seg_btn[k] = chip(s_seg, seg_clicked, k, &l);
        lv_obj_set_size(s_seg_btn[k], 70, 22);
        lv_obj_set_style_pad_hor(s_seg_btn[k], 0, 0);
    }
    s_sub = label(s_root, "", &lv_font_es_12, COL_MUTED);
    lv_obj_set_width(s_sub, 224);
    lv_obj_set_style_text_align(s_sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_sub, 8, SUB_Y + 3);

    s_grid = lv_obj_create(s_root);
    lv_obj_set_size(s_grid, 240, UI_NAV_Y - GRID_Y);
    lv_obj_set_pos(s_grid, 0, GRID_Y);
    lv_obj_set_style_bg_opa(s_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_grid, 0, 0);
    lv_obj_set_style_radius(s_grid, 0, 0);
    lv_obj_set_style_pad_left(s_grid, 9, 0);
    lv_obj_set_style_pad_right(s_grid, 7, 0);
    lv_obj_set_style_pad_top(s_grid, 2, 0);
    lv_obj_set_style_pad_bottom(s_grid, 6, 0); /* la sombra de la ultima fila */
    lv_obj_set_style_pad_row(s_grid, 8, 0);
    lv_obj_set_style_pad_column(s_grid, 6, 0);
    lv_obj_set_flex_flow(s_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_scroll_dir(s_grid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_grid, LV_SCROLLBAR_MODE_ACTIVE);

    s_try = box(s_root); /* Probar: tapa pestañas y rejilla */
    lv_obj_set_size(s_try, 240, UI_NAV_Y - TOP_H);
    lv_obj_set_pos(s_try, 0, TOP_H);
    lv_obj_set_style_bg_color(s_try, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_try, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_try, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_clickable(s_try, true); /* que un toque no llegue a las pestañas de debajo */
    lv_obj_set_hidden(s_try, true);

    lv_obj_set_hidden(s_root, true);
    return s_root;
}

void ui_shop_open(void)
{
    if (!s_root) return;
    s_try_item = RW_ITEM_COUNT;
    lv_obj_set_hidden(s_root, false);
    lv_obj_move_foreground(s_root);
    ui_tel_screen(UI_TEL_SHOP);
    ui_shop_refresh();
}

void ui_shop_refresh(void)
{
    if (!s_root || lv_obj_is_hidden(s_root)) return;
    const rw_state_t *r = ui_rewards();
    lv_label_set_text_fmt(s_bal, "%u", (unsigned)r->balance);
    bool trying = s_try_item < RW_ITEM_COUNT;
    lv_label_set_text(s_title, trying ? ui_item_name(s_try_item) : tr(STR_SHOP));
    lv_obj_set_hidden(s_try, !trying);
    if (trying) {
        try_build();
        lv_obj_clean(s_grid);
        return;
    }
    tabs_apply();
    grid_build();
}

bool ui_shop_visible(void) { return s_root && !lv_obj_is_hidden(s_root); }

/* «Una IncuNest con tu nombre», desde su tarjeta de Corazones: el mismo canje de siempre */
void ui_shop_special_clicked(void) { act_on(RW_ITEM_NAMED_INCUNEST); }

/* Consola (ui_debug): pestaña `tab` (-1 = cerrar), Probar `try_item` (-1 = rejilla) y su dialogo */
void ui_shop_debug(int tab, bool girl, int try_item, bool buy)
{
    if (!s_root) return;
    if (tab < 0 || tab >= TAB_N) {
        /* los dialogos: el msgbox va dentro de su fondo oscuro, hijo de la capa superior */
        for (int32_t i = (int32_t)lv_obj_get_child_count(lv_layer_top()) - 1; i >= 0; i--) {
            lv_obj_t *c = lv_obj_get_child(lv_layer_top(), i);
            lv_obj_t *mb = lv_obj_check_type(c, &lv_msgbox_class) ? c : NULL;
            for (uint32_t k = 0; !mb && k < lv_obj_get_child_count(c); k++) {
                lv_obj_t *g = lv_obj_get_child(c, (int32_t)k);
                if (lv_obj_check_type(g, &lv_msgbox_class)) mb = g;
            }
            if (mb) lv_msgbox_close(mb);
        }
        s_try_item = RW_ITEM_COUNT;
        lv_obj_set_hidden(s_root, true);
        lv_obj_clean(s_grid);
        return;
    }
    s_tab = (shop_tab_t)tab;
    s_girl = girl;
    lv_obj_set_hidden(s_root, false);
    lv_obj_move_foreground(s_root);
    ui_tel_screen(UI_TEL_SHOP);
    s_try_item = try_item >= 0 && try_item < RW_ITEM_COUNT ? (rw_item_t)try_item : RW_ITEM_COUNT;
    lv_obj_scroll_to_y(s_grid, 0, LV_ANIM_OFF);
    ui_shop_refresh();
    if (buy) act_on(s_try_item < RW_ITEM_COUNT ? s_try_item : RW_ITEM_HAT_BOY_MINT);
}
