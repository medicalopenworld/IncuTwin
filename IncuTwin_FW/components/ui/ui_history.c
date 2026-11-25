/* Pantalla de Historial (spec hmi-navegacion-avisos): los ultimos avisos guardados en el panel
 * (event_log), del mas reciente al mas antiguo, con icono, texto y tiempo relativo. Vacia: el
 * pollito y una frase. Titulo arriba (y 0..40), lista con scroll en y 40..280 y la barra debajo.
 * event_log toma un mutex (y una escritura en NVS puede tenerlo unos ms): solo se lee al cambiar
 * el historial, al abrir la pantalla y una vez por minuto mientras se ve (los «hace N min»). */
#include <time.h>

#include "assets.h"
#include "event_log.h"
#include "settings.h"
#include "ui_internal.h"
#include "ui_rel_time.h"

#define TITLE_H  40
#define HIST_LIST_H   (UI_NAV_Y - TITLE_H)
#define BADGE_D  34
#define ROW_W    224
#define TEXT_W   (ROW_W - BADGE_D - 8)
#define TIME_VALID_EPOCH 1700000000u /* antes de esto el reloj no esta en hora */

static lv_obj_t *s_scr, *s_title, *s_list, *s_empty, *s_empty_lbl;

/* ------------------------------------------------------------- filas */

/* Circulo del icono, como los del tratamiento en Inicio: activo = relleno de su color con el
 * icono en blanco; el que termina, blanco con el icono atenuado. */
static lv_obj_t *badge(lv_obj_t *row, lv_color_t col, bool filled)
{
    lv_obj_t *c = lv_obj_create(row);
    lv_obj_set_size(c, BADGE_D, BADGE_D);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c, filled ? col : COL_CARD, 0);
    lv_obj_set_style_border_width(c, 2, 0);
    lv_obj_set_style_border_color(c, filled ? col : COL_OFFLINE, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_set_scrollable(c, false);
    lv_obj_set_clickable(c, false);
    return c;
}

static void badge_image(lv_obj_t *c, const lv_image_dsc_t *img, bool white, lv_opa_t opa, uint16_t scale)
{
    lv_obj_t *i = lv_image_create(c);
    lv_image_set_src(i, img);
    if (scale != LV_SCALE_NONE) lv_image_set_scale(i, scale);
    if (white) {
        lv_obj_set_style_image_recolor(i, lv_color_white(), 0);
        lv_obj_set_style_image_recolor_opa(i, LV_OPA_COVER, 0);
    }
    lv_obj_set_style_image_opa(i, opa, 0);
    lv_obj_center(i);
}

static void badge_symbol(lv_obj_t *c, const char *sym, lv_color_t col)
{
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, sym);
    lv_obj_set_style_text_font(l, &lv_font_es_16, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_center(l);
}

/* Compartido con la franja de aviso (ui_notice.c) para que icono y color coincidan. */
void ui_notice_icon(lv_obj_t *row, uint8_t kind)
{
    lv_obj_t *c;
    switch (kind) {
    case TWIN_NOTICE_ADMISSION: /* llega un bebe: el pollito */
        c = badge(row, COL_CORAL, false);
        lv_obj_set_style_border_color(c, COL_CORAL, 0);
        badge_image(c, &img_chick_28, false, LV_OPA_COVER, LV_SCALE_NONE);
        break;
    case TWIN_NOTICE_PARENTS:
        c = badge(row, COL_CORAL, false);
        lv_obj_set_style_border_color(c, COL_CORAL, 0);
        badge_image(c, &img_heart, false, LV_OPA_COVER, 180); /* 34x30 a ~24x21 */
        break;
    case TWIN_NOTICE_HEAT_ON:
    case TWIN_NOTICE_HEAT_OFF: {
        bool on = kind == TWIN_NOTICE_HEAT_ON;
        c = badge(row, COL_WARM, on);
        badge_image(c, &img_icon_thermo, on, on ? LV_OPA_COVER : LV_OPA_50, 220);
        break;
    }
    case TWIN_NOTICE_PHOTO_ON:
    case TWIN_NOTICE_PHOTO_OFF: {
        bool on = kind == TWIN_NOTICE_PHOTO_ON;
        c = badge(row, COL_PHOTO, on);
        badge_image(c, &img_icon_lamp, on, on ? LV_OPA_COVER : LV_OPA_50, 220);
        break;
    }
    default: /* salida: neutro, sin colores de alarma */
        c = badge(row, COL_OFFLINE, false);
        badge_symbol(c, LV_SYMBOL_RIGHT, COL_OFFLINE);
        break;
    }
}

static void row_create(const event_entry_t *e, uint32_t now, uint8_t lang)
{
    lv_obj_t *row = lv_obj_create(s_list);
    lv_obj_set_size(row, ROW_W, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(row, COL_CARD, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 6, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(row, false);
    lv_obj_set_clickable(row, false); /* el dedo arrastra la lista */
    ui_notice_icon(row, e->kind);

    lv_obj_t *col = lv_obj_create(row);
    lv_obj_set_size(col, TEXT_W - 12, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_style_pad_row(col, 1, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollable(col, false);
    lv_obj_set_clickable(col, false);

    lv_obj_t *txt = lv_label_create(col);
    lv_obj_set_width(txt, LV_PCT(100));
    lv_label_set_long_mode(txt, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_font(txt, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(txt, COL_NAVY_DARK, 0);
    lv_label_set_text(txt, ui_notice_text((twin_notice_t)e->kind));

    char when[24];
    ui_rel_time(now ? e->at : 0, now, when, sizeof(when), lang); /* sin reloj en hora: «-» */
    lv_obj_t *t = lv_label_create(col);
    lv_obj_set_style_text_font(t, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(t, COL_OFFLINE, 0);
    lv_label_set_text(t, when);
}

/* ----------------------------------------------------------------- build */

lv_obj_t *ui_history_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_scrollable(s_scr, false);

    s_title = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_title, &lv_font_es_20, 0);
    lv_obj_set_style_text_color(s_title, COL_NAVY, 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 9);

    s_list = lv_obj_create(s_scr);
    lv_obj_set_size(s_list, 240, HIST_LIST_H);
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

    /* vacio: pollito y frase, centrados en la zona de la lista */
    s_empty = lv_obj_create(s_scr);
    lv_obj_set_size(s_empty, 240, HIST_LIST_H);
    lv_obj_set_pos(s_empty, 0, TITLE_H);
    lv_obj_set_style_bg_opa(s_empty, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_empty, 0, 0);
    lv_obj_set_style_pad_all(s_empty, 0, 0);
    lv_obj_set_scrollable(s_empty, false);
    lv_obj_set_clickable(s_empty, false);
    lv_obj_t *chick = lv_image_create(s_empty);
    lv_image_set_src(chick, &img_chick_64);
    lv_obj_align(chick, LV_ALIGN_CENTER, 0, -36);
    s_empty_lbl = lv_label_create(s_empty);
    lv_obj_set_width(s_empty_lbl, 200);
    lv_label_set_long_mode(s_empty_lbl, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_font(s_empty_lbl, &lv_font_es_16, 0);
    lv_obj_set_style_text_color(s_empty_lbl, COL_NAVY_DARK, 0);
    lv_obj_set_style_text_align(s_empty_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_empty_lbl, LV_ALIGN_CENTER, 0, 30);

    ui_nav_create(s_scr, UI_TAB_HISTORY);
    ui_history_retranslate(); /* incluye ui_history_refresh() */
    return s_scr;
}

void ui_history_retranslate(void)
{
    lv_label_set_text(s_title, tr(STR_HISTORY));
    lv_label_set_text(s_empty_lbl, tr(STR_HISTORY_EMPTY));
    ui_history_refresh(); /* los textos de las filas y los «hace N min» */
}

void ui_history_refresh(void)
{
    uint8_t n = event_log_count();
    lv_obj_set_hidden(s_empty, n > 0);
    lv_obj_set_hidden(s_list, n == 0);

    int32_t sy = lv_obj_get_scroll_y(s_list); /* que el refresco de cada minuto no te suba arriba */
    lv_obj_clean(s_list);
    time_t t = time(NULL);
    uint32_t now = t >= (time_t)TIME_VALID_EPOCH ? (uint32_t)t : 0;
    uint8_t lang = settings_lang();
    event_entry_t e;
    for (uint8_t i = 0; i < n && event_log_get(i, &e); i++) {
        row_create(&e, now, lang);
    }
    if (n > 0) {
        lv_obj_update_layout(s_list);
        lv_obj_scroll_to_y(s_list, sy, LV_ANIM_OFF);
    }
}
