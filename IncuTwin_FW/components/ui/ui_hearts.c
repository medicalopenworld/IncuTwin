/* Pantalla Corazones (spec twin-display "Pantalla Corazones", spec rewards): todo lo que se gana
 * acompañando, en una lista con scroll:
 *   la tienda: una tarjeta que abre la pantalla Tienda (ui_shop.c), lo primero
 *   saldo y nivel (con lo que falta para el siguiente)
 *   tu racha: la semana (lunes a domingo) y los dias seguidos, con los abrazos guardados
 *   sellos: los siete; tocar uno dice que es y como se consigue
 *   pasaporte: la bandera de cada pais (corazon dorado si un bebe de alli se fue a casa)
 * Los cambios van por `rewards`, que publica TWIN_EVT_REWARDS_CHANGED: la lista se rehace en el
 * refresco de ui.c, nunca dentro del evento de un toque (la fila tocada se borraria con ella). */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "assets.h"
#include "net_wifi.h"
#include "rewards.h"
#include "ui_internal.h"
#include "ui_toy.h"

#define TITLE_H      40
#define HEARTS_LIST_H (UI_NAV_Y - TITLE_H)
#define ROW_W        224
#define SEAL_D       40
#define WEEK_N       7

static lv_obj_t *s_scr, *s_title, *s_bal, *s_list;

/* ------------------------------------------------------------- piezas */

static lv_obj_t *box(lv_obj_t *parent, lv_flex_flow_t flow)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_radius(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_set_flex_flow(b, flow);
    lv_obj_set_scrollable(b, false);
    lv_obj_set_clickable(b, false);
    return b;
}

static lv_obj_t *card(void)
{
    lv_obj_t *c = box(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(c, ROW_W, LV_SIZE_CONTENT);
    toy_box(c, TOY_R_CARD, TOY_SHADOW_M); /* tarjeta de juguete, como la app */
    lv_obj_set_style_pad_all(c, 8, 0);
    lv_obj_set_style_pad_row(c, 6, 0);
    return c;
}

static lv_obj_t *label(lv_obj_t *parent, const char *t, const lv_font_t *f, lv_color_t col)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, col, 0);
    return l;
}

static void section(ui_str_t id)
{
    lv_obj_t *l = label(s_list, tr(id), &lv_font_title_16, COL_INK);
    lv_obj_set_style_pad_top(l, 6, 0);
}

/* Corazon de pixeles de la app: 9x8 celdas de `cell` px */
static lv_obj_t *heart_img(lv_obj_t *parent, toy_heart_t kind, int32_t cell)
{
    return toy_heart_img(parent, kind, cell);
}

static bool now_local(struct tm *tm, int32_t *day)
{
    if (!net_wifi_time_valid()) return false;
    time_t t = time(NULL);
    localtime_r(&t, tm);
    *day = rw_day_of(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    return true;
}

/* ------------------------------------------------------------- handlers */

static void close_mb(lv_event_t *e) { lv_msgbox_close(lv_event_get_user_data(e)); }

static void seal_clicked(lv_event_t *e)
{
    rw_seal_t seal = (rw_seal_t)(intptr_t)lv_event_get_user_data(e);
    ui_msgbox(ui_seal_name(seal), ui_seal_desc(seal), tr(STR_OK), COL_NAVY, close_mb, NULL);
}

/* ------------------------------------------------------------- bloques */

/* Saldo grande, nivel y una barra hasta el siguiente nivel */
static void level_card(const rw_state_t *r)
{
    lv_obj_t *c = card();
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(c, 10, 0);
    heart_img(c, TOY_HEART_FULL, 4); /* 36x32 */
    lv_obj_t *col = box(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(col, ROW_W - 20 - 36 - 10, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(col, 2, 0);
    char buf[48];
    lv_obj_t *top = box(col, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(top, 8, 0);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    snprintf(buf, sizeof(buf), "%u", (unsigned)r->balance);
    label(top, buf, &lv_font_title_24, COL_INK);
    rw_level_t lv = rw_level(r->earned);
    label(top, ui_level_name(lv), &lv_font_title_16, COL_INK);
    /* barra de juguete: pista coral suave con borde tinta, relleno coral */
    lv_obj_t *bar = lv_bar_create(col);
    lv_obj_set_size(bar, LV_PCT(100), 12);
    lv_obj_set_style_bg_color(bar, COL_CORAL_SOFT, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, COL_INK, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, TOY_BORDER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COL_CORAL, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
    if (lv + 1 < RW_LEVEL_COUNT) {
        uint32_t lo = rw_level_min(lv), hi = rw_level_min((rw_level_t)(lv + 1));
        lv_bar_set_range(bar, 0, (int32_t)(hi - lo));
        lv_bar_set_value(bar, (int32_t)(r->earned - lo), LV_ANIM_OFF);
        snprintf(buf, sizeof(buf), tr(STR_HEARTS_NEXT), (unsigned)(hi - r->earned),
                 ui_level_name((rw_level_t)(lv + 1)));
    } else {
        lv_bar_set_range(bar, 0, 1);
        lv_bar_set_value(bar, 1, LV_ANIM_OFF);
        snprintf(buf, sizeof(buf), "%s", tr(STR_HEARTS_MAX));
    }
    label(col, buf, &lv_font_es_12, COL_MUTED);
}

/* La semana de lunes a domingo a partir de la racha: los dias seguidos que acaban en el ultimo
 * gesto (si fue hoy o ayer); dorado el de hoy si hubo super corazon */
static void streak_card(const rw_state_t *r)
{
    lv_obj_t *c = card();
    struct tm tm;
    int32_t today = 0;
    bool valid = now_local(&tm, &today);
    int idx = valid ? (tm.tm_wday + 6) % 7 : 6; /* 0 = lunes */
    lv_obj_t *week = box(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(week, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(week, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const char *ini = tr(STR_WEEKDAY_INITIALS);
    bool ini_ok = strlen(ini) == 7;
    int32_t gap = valid && r->last_day != RW_DAY_NONE ? today - r->last_day : 99;
    for (int i = 0; i < WEEK_N; i++) {
        int off = idx - i; /* dias atras; < 0 = aun no ha llegado */
        bool on = valid && off >= 0 && (gap == 0 || gap == 1) && off >= gap && off < gap + r->streak;
        bool super = on && off == 0 && r->super_day == today;
        lv_obj_t *d = box(week, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_size(d, 28, LV_SIZE_CONTENT);
        lv_obj_set_flex_align(d, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(d, 2, 0);
        lv_obj_t *h = lv_image_create(d);
        ui_week_heart_style(h, on, super, false, valid && off < 0);
        lv_obj_set_size(h, 27, 24); /* celdas de 3 px */
        char s[2] = { valid && ini_ok ? ini[(i + 1) % 7] : '\0', '\0' };
        lv_obj_t *l = label(d, s, &lv_font_es_12, valid && off == 0 ? COL_CORAL_INK : COL_MUTED);
        (void)l;
    }
    lv_obj_t *line = box(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(line, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(line, 5, 0);
    lv_obj_set_flex_align(line, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    char buf[40];
    unsigned run = (gap == 0 || gap == 1) ? r->streak : 0;
    snprintf(buf, sizeof(buf), tr(STR_HEARTS_STREAK), run);
    label(line, buf, &lv_font_es_14, COL_INK);
    for (int i = 0; i < r->saved; i++) heart_img(line, TOY_HEART_REST, 2); /* abrazos guardados */
}

static void seals_card(const rw_state_t *r)
{
    lv_obj_t *c = card();
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(c, 10, 0);
    for (int i = 0; i < RW_SEAL_COUNT; i++) {
        lv_obj_t *slot = box(c, LV_FLEX_FLOW_ROW);
        lv_obj_set_size(slot, SEAL_D + 6, SEAL_D);
        lv_obj_set_layout(slot, LV_LAYOUT_NONE);
        lv_obj_set_clickable(slot, true);
        lv_obj_add_flag(slot, UI_FLAG_CLICK_SOUND);
        lv_obj_add_event_cb(slot, seal_clicked, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *icon = ui_seal_icon(slot, (rw_seal_t)i, r->seals & (1u << i), SEAL_D);
        lv_obj_set_pos(icon, 3, 0);
    }
}

static void passport_card(const rw_state_t *r)
{
    lv_obj_t *c = card();
    if (!r->n_countries) {
        lv_obj_t *l = label(c, tr(STR_PASSPORT_EMPTY), &lv_font_es_12, COL_MUTED);
        lv_obj_set_width(l, LV_PCT(100));
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
        return;
    }
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(c, 6, 0);
    for (int i = 0; i < r->n_countries; i++) {
        const rw_country_t *p = &r->passport[i];
        char iso[3] = { p->iso[0], p->iso[1], 0 };
        lv_obj_t *b = box(c, LV_FLEX_FLOW_ROW);
        lv_obj_set_size(b, 34, 25);
        lv_obj_set_layout(b, LV_LAYOUT_NONE);
        const lv_image_dsc_t *flag = flag_image(iso);
        if (flag) {
            lv_obj_t *f = lv_image_create(b);
            lv_image_set_src(f, flag);
            lv_image_set_inner_align(f, LV_IMAGE_ALIGN_STRETCH);
            lv_obj_set_size(f, 30, 20);
            lv_obj_set_pos(f, 0, 5);
        } else {
            lv_obj_set_pos(label(b, iso, &lv_font_es_12, COL_NAVY_DARK), 2, 8);
        }
        if (p->star) {
            lv_obj_t *st = heart_img(b, TOY_HEART_SUPER, 2);
            lv_obj_set_pos(st, 18, 0);
        }
    }
}

/* La tienda: una tarjeta que la abre */
static void shop_clicked(lv_event_t *e)
{
    (void)e;
    ui_shop_open();
}

static void shop_card(void)
{
    lv_obj_t *row = lv_obj_create(s_list);
    lv_obj_set_size(row, ROW_W, LV_SIZE_CONTENT);
    toy_box(row, TOY_R_CARD, TOY_SHADOW_M); /* se hunde al pulsarla */
    lv_obj_set_style_pad_ver(row, 8, 0);
    lv_obj_set_style_pad_hor(row, 10, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(row, false);
    lv_obj_add_flag(row, UI_FLAG_CLICK_SOUND);
    lv_obj_add_event_cb(row, shop_clicked, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_t *col = box(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    label(col, tr(STR_SHOP), &lv_font_title_16, COL_INK);
    lv_obj_set_style_pad_row(col, 1, 0);
    label(col, tr(STR_SHOP_ENTRY), &lv_font_es_12, COL_MUTED);
    label(row, LV_SYMBOL_RIGHT, &lv_font_es_16, COL_INK);
}

/* «Una IncuNest con tu nombre» (el canje especial), debajo de la tienda */
static void special_clicked(lv_event_t *e)
{
    (void)e;
    ui_shop_special_clicked();
}

static void special_card(const rw_state_t *r)
{
    bool own = r->unlocked & RW_BIT(RW_ITEM_NAMED_INCUNEST);
    lv_obj_t *row = lv_obj_create(s_list);
    lv_obj_set_size(row, ROW_W, LV_SIZE_CONTENT);
    toy_box(row, TOY_R_CARD, TOY_SHADOW_M);
    lv_obj_set_style_pad_ver(row, 6, 0);
    lv_obj_set_style_pad_hor(row, 8, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(row, false);
    lv_obj_add_flag(row, UI_FLAG_CLICK_SOUND);
    lv_obj_add_event_cb(row, special_clicked, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_t *n = lv_image_create(row);
    lv_image_set_src(n, &img_incunest_28);
    lv_obj_set_clickable(n, false);
    lv_obj_t *name = label(row, ui_item_name(RW_ITEM_NAMED_INCUNEST), &lv_font_title_14, COL_INK);
    lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_flex_grow(name, 1);
    if (own) {
        label(row, tr(STR_REDEEMED), &lv_font_es_14, COL_OK);
    } else {
        heart_img(row, TOY_HEART_FULL, 2);
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", (unsigned)rw_price(RW_ITEM_NAMED_INCUNEST));
        label(row, buf, &lv_font_es_14, r->balance >= rw_price(RW_ITEM_NAMED_INCUNEST) ? COL_CORAL_INK : COL_MUTED);
    }
}

/* ----------------------------------------------------------------- build */

lv_obj_t *ui_hearts_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_scrollable(s_scr, false);

    s_title = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_title, &lv_font_title_20, 0);
    lv_obj_set_style_text_color(s_title, COL_INK, 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 9);

    /* saldo arriba a la derecha (se ve aunque se baje a la tienda) */
    lv_obj_t *bal = box(s_scr, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(bal, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_pad_column(bal, 4, 0);
    lv_obj_set_flex_align(bal, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(bal, LV_ALIGN_TOP_RIGHT, -8, 8);
    heart_img(bal, TOY_HEART_FULL, 2);
    s_bal = label(bal, "", &lv_font_title_16, COL_INK);

    s_list = lv_obj_create(s_scr);
    lv_obj_set_size(s_list, 240, HEARTS_LIST_H);
    lv_obj_set_pos(s_list, 0, TITLE_H);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_radius(s_list, 0, 0);
    lv_obj_set_style_pad_hor(s_list, 8, 0);
    lv_obj_set_style_pad_top(s_list, 4, 0);
    lv_obj_set_style_pad_bottom(s_list, 8, 0); /* la sombra de la ultima tarjeta */
    lv_obj_set_style_pad_row(s_list, 9, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_ACTIVE);

    ui_hearts_refresh();
    return s_scr;
}

void ui_hearts_refresh(void)
{
    if (!s_scr) return;
    const rw_state_t *r = ui_rewards();
    lv_label_set_text(s_title, tr(STR_HEARTS));
    lv_label_set_text_fmt(s_bal, "%u", (unsigned)r->balance);
    int32_t sy = lv_obj_get_scroll_y(s_list);
    lv_obj_clean(s_list);
    shop_card(); /* la tienda, lo primero (Pablo, 26-nov) */
    special_card(r);
    level_card(r);
    section(STR_STREAK_TITLE);
    streak_card(r);
    section(STR_SEALS);
    seals_card(r);
    section(STR_PASSPORT);
    passport_card(r);
    lv_obj_update_layout(s_list);
    lv_obj_scroll_to_y(s_list, sy, LV_ANIM_OFF);
}

lv_obj_t *ui_hearts_screen(void) { return s_scr; }

void ui_hearts_show_top(void)
{
    if (s_list) lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);
}
