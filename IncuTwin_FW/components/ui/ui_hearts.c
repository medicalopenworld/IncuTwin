/* Pantalla Corazones (spec twin-display "Pantalla Corazones", spec rewards): todo lo que se gana
 * acompañando, en una lista con scroll:
 *   saldo y nivel (con lo que falta para el siguiente)
 *   tu racha: la semana (lunes a domingo) y los dias seguidos, con los abrazos guardados
 *   sellos: los siete; tocar uno dice que es y como se consigue
 *   pasaporte: la bandera de cada pais (corazon dorado si un bebe de alli se fue a casa)
 *   tienda: halo, fondo, la guirnalda en su temporada y «Una IncuNest con tu nombre»
 * Los cambios van por `rewards`, que publica TWIN_EVT_REWARDS_CHANGED: la lista se rehace en el
 * refresco de ui.c, nunca dentro del evento de un toque (la fila tocada se borraria con ella). */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "assets.h"
#include "net_wifi.h"
#include "rewards.h"
#include "ui_internal.h"

#define TITLE_H      40
#define HEARTS_LIST_H (UI_NAV_Y - TITLE_H)
#define ROW_W        224
#define SEAL_D       40
#define WEEK_N       7

static lv_obj_t *s_scr, *s_title, *s_bal, *s_list;
static rw_item_t s_pending; /* la que se va a desbloquear o canjear (dialogo abierto) */

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
    lv_obj_set_style_bg_color(c, COL_CARD, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, 12, 0);
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
    lv_obj_t *l = label(s_list, tr(id), &lv_font_es_16, COL_NAVY);
    lv_obj_set_style_pad_top(l, 6, 0);
}

static lv_obj_t *heart_img(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_t *i = lv_image_create(parent);
    lv_image_set_src(i, &img_heart);
    lv_image_set_inner_align(i, LV_IMAGE_ALIGN_STRETCH);
    lv_obj_set_size(i, w, h);
    lv_obj_set_clickable(i, false);
    return i;
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

static void unlock_confirmed(lv_event_t *e)
{
    lv_msgbox_close(lv_event_get_user_data(e));
    uint16_t missing = 0;
    rewards_unlock(s_pending, &missing);
}

static void seal_clicked(lv_event_t *e)
{
    rw_seal_t seal = (rw_seal_t)(intptr_t)lv_event_get_user_data(e);
    ui_msgbox(ui_seal_name(seal), ui_seal_desc(seal), tr(STR_OK), COL_NAVY, close_mb, NULL);
}

static void item_clicked(lv_event_t *e)
{
    rw_item_t item = (rw_item_t)(intptr_t)lv_event_get_user_data(e);
    const rw_state_t *r = ui_rewards();
    char txt[200];
    bool own = r->unlocked & (1u << item);
    if (item == RW_ITEM_NAMED_INCUNEST && own) return; /* ya pedida */
    if (own) {
        uint16_t missing;
        rewards_unlock(item, &missing); /* elegir (la guirnalda: poner / quitar) */
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
        ui_msgbox(ui_item_name(item), txt, tr(STR_REDEEM), COL_CORAL, unlock_confirmed, NULL);
    } else {
        snprintf(txt, sizeof(txt), tr(STR_CUST_UNLOCK_Q), ui_item_name(item), (unsigned)price);
        ui_msgbox(ui_item_name(item), txt, tr(STR_CUST_UNLOCK), COL_CORAL, unlock_confirmed, NULL);
    }
}

/* ------------------------------------------------------------- bloques */

/* Saldo grande, nivel y una barra hasta el siguiente nivel */
static void level_card(const rw_state_t *r)
{
    lv_obj_t *c = card();
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(c, 10, 0);
    heart_img(c, 40, 35);
    lv_obj_t *col = box(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(col, ROW_W - 16 - 40 - 10, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(col, 2, 0);
    char buf[48];
    lv_obj_t *top = box(col, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(top, 8, 0);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    snprintf(buf, sizeof(buf), "%u", (unsigned)r->balance);
    label(top, buf, &lv_font_es_20, COL_CORAL);
    rw_level_t lv = rw_level(r->earned);
    label(top, ui_level_name(lv), &lv_font_es_16, COL_NAVY_DARK);
    lv_obj_t *bar = lv_bar_create(col);
    lv_obj_set_size(bar, LV_PCT(100), 8);
    lv_obj_set_style_bg_color(bar, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COL_CORAL, LV_PART_INDICATOR);
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
    label(col, buf, &lv_font_es_12, COL_OFFLINE);
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
        lv_obj_set_size(d, 26, LV_SIZE_CONTENT);
        lv_obj_set_flex_align(d, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(d, 2, 0);
        ui_week_heart_style(heart_img(d, 24, 21), on, super, false, valid && off < 0);
        char s[2] = { valid && ini_ok ? ini[(i + 1) % 7] : '\0', '\0' };
        lv_obj_t *l = label(d, s, &lv_font_es_12, valid && off == 0 ? COL_CORAL : COL_OFFLINE);
        (void)l;
    }
    lv_obj_t *line = box(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(line, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(line, 5, 0);
    lv_obj_set_flex_align(line, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    char buf[40];
    unsigned run = (gap == 0 || gap == 1) ? r->streak : 0;
    snprintf(buf, sizeof(buf), tr(STR_HEARTS_STREAK), run);
    label(line, buf, &lv_font_es_14, COL_NAVY_DARK);
    for (int i = 0; i < r->saved; i++) { /* abrazos guardados */
        lv_obj_t *h = heart_img(line, 15, 13);
        lv_obj_set_style_image_recolor(h, COL_REST, 0);
        lv_obj_set_style_image_recolor_opa(h, LV_OPA_COVER, 0);
    }
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
        lv_obj_t *l = label(c, tr(STR_PASSPORT_EMPTY), &lv_font_es_12, COL_OFFLINE);
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
            lv_obj_t *st = heart_img(b, 13, 11);
            lv_obj_set_pos(st, 21, 0);
            lv_obj_set_style_image_recolor(st, COL_SUPER, 0);
            lv_obj_set_style_image_recolor_opa(st, LV_OPA_COVER, 0);
        }
    }
}

static void group_label(ui_str_t id)
{
    lv_obj_t *l = label(s_list, tr(id), &lv_font_es_12, COL_NAVY_DARK);
    lv_obj_set_style_pad_top(l, 2, 0);
}

static void item_row(rw_item_t item, const rw_state_t *r)
{
    bool own = r->unlocked & (1u << item);
    bool named = item == RW_ITEM_NAMED_INCUNEST;
    bool chosen = item == RW_ITEM_GARLAND ? (own && r->garland_on) : !named && (r->halo == item || r->bg == item);

    lv_obj_t *row = lv_obj_create(s_list);
    lv_obj_set_size(row, ROW_W, named ? LV_SIZE_CONTENT : 40);
    lv_obj_set_style_min_height(row, 40, 0);
    lv_obj_set_style_bg_color(row, COL_CARD, 0);
    lv_obj_set_style_bg_color(row, COL_BG_PHOTO, LV_STATE_PRESSED);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_border_width(row, chosen || named ? 2 : 0, 0);
    lv_obj_set_style_border_color(row, named ? COL_SUPER : COL_CORAL, 0);
    lv_obj_set_style_pad_hor(row, 8, 0);
    lv_obj_set_style_pad_ver(row, named ? 6 : 0, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(row, false);
    lv_obj_add_flag(row, UI_FLAG_CLICK_SOUND);
    lv_obj_add_event_cb(row, item_clicked, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)item);

    /* muestra: el color; la guirnalda, un trocito; la IncuNest, la IncuNest */
    if (named) {
        lv_obj_t *n = lv_image_create(row);
        lv_image_set_src(n, &img_incunest_28);
        lv_obj_set_clickable(n, false);
    } else {
        lv_obj_t *sw = lv_obj_create(row);
        lv_obj_set_size(sw, 26, 26);
        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(sw, item == RW_ITEM_GARLAND ? COL_CARD : ui_item_color(item), 0);
        lv_obj_set_style_border_width(sw, 1, 0);
        lv_obj_set_style_border_color(sw, COL_OFFLINE, 0);
        lv_obj_set_style_pad_all(sw, 0, 0);
        lv_obj_set_scrollable(sw, false);
        lv_obj_set_clickable(sw, false);
        if (item == RW_ITEM_GARLAND) {
            lv_obj_t *g = lv_image_create(sw);
            lv_image_set_src(g, &img_garland);
            lv_image_set_offset_x(g, -70);
            lv_obj_set_size(g, 24, 22);
            lv_obj_center(g);
        }
    }

    lv_obj_t *name = label(row, ui_item_name(item), &lv_font_es_16, COL_NAVY_DARK);
    lv_obj_set_flex_grow(name, 1);
    if (named) {
        lv_obj_set_style_text_font(name, &lv_font_es_14, 0);
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_WRAP);
    }

    if (named && own) {
        label(row, tr(STR_REDEEMED), &lv_font_es_14, COL_OK);
    } else if (own) {
        label(row, tr(chosen ? STR_CUST_CHOSEN : STR_CUST_USE), &lv_font_es_14, chosen ? COL_CORAL : COL_NAVY);
    } else if (item == RW_ITEM_GARLAND) { /* lo que llevas de diciembre: «3/10» */
        unsigned days = 0;
        struct tm tm;
        int32_t today;
        if (now_local(&tm, &today) && r->dec_year == (uint16_t)(tm.tm_year + 1900)) {
            days = (unsigned)__builtin_popcount(r->dec_days);
        }
        char buf[8];
        snprintf(buf, sizeof(buf), "%u/10", days);
        label(row, buf, &lv_font_es_14, COL_OFFLINE);
    } else {
        heart_img(row, 16, 14);
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", (unsigned)rw_price(item));
        label(row, buf, &lv_font_es_16, r->balance >= rw_price(item) ? COL_CORAL : COL_OFFLINE);
    }
}

/* La guirnalda sale en su temporada (1-dic .. 6-ene) o si ya se tiene */
static bool garland_listed(const rw_state_t *r)
{
    if (r->unlocked & (1u << RW_ITEM_GARLAND)) return true;
    struct tm tm;
    int32_t today;
    return now_local(&tm, &today) && rw_garland_season(tm.tm_mon + 1, tm.tm_mday);
}

/* ----------------------------------------------------------------- build */

lv_obj_t *ui_hearts_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_scrollable(s_scr, false);

    s_title = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_title, &lv_font_es_20, 0);
    lv_obj_set_style_text_color(s_title, COL_NAVY, 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 9);

    /* saldo arriba a la derecha (se ve aunque se baje a la tienda) */
    lv_obj_t *bal = box(s_scr, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(bal, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_pad_column(bal, 4, 0);
    lv_obj_set_flex_align(bal, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(bal, LV_ALIGN_TOP_RIGHT, -8, 8);
    heart_img(bal, 18, 16);
    s_bal = label(bal, "", &lv_font_es_16, COL_CORAL);

    s_list = lv_obj_create(s_scr);
    lv_obj_set_size(s_list, 240, HEARTS_LIST_H);
    lv_obj_set_pos(s_list, 0, TITLE_H);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_radius(s_list, 0, 0);
    lv_obj_set_style_pad_hor(s_list, 8, 0);
    lv_obj_set_style_pad_ver(s_list, 4, 0);
    lv_obj_set_style_pad_row(s_list, 6, 0);
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
    level_card(r);
    section(STR_STREAK_TITLE);
    streak_card(r);
    section(STR_SEALS);
    seals_card(r);
    section(STR_PASSPORT);
    passport_card(r);
    section(STR_SHOP);
    group_label(STR_CUST_SPECIAL);
    item_row(RW_ITEM_NAMED_INCUNEST, r);
    group_label(STR_CUST_HALO);
    for (int i = RW_HALO_FIRST; i <= RW_HALO_LAST; i++) item_row((rw_item_t)i, r);
    group_label(STR_CUST_BG);
    for (int i = RW_BG_FIRST; i <= RW_BG_LAST; i++) item_row((rw_item_t)i, r);
    if (garland_listed(r)) {
        group_label(STR_CUST_SEASON);
        item_row(RW_ITEM_GARLAND, r);
    }
    lv_obj_update_layout(s_list);
    lv_obj_scroll_to_y(s_list, sy, LV_ANIM_OFF);
}

void ui_hearts_show_top(void)
{
    if (s_list) lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);
}
