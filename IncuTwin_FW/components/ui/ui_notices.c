/* Pantalla de Notificaciones (spec hmi-navegacion-avisos): todos los cambios de estado guardados
 * en el panel (event_log), del mas reciente al mas antiguo, con icono, texto y tiempo relativo.
 * Los avisos de entrada y salida llevan al bebe, nino o nina segun su sexo. Si hay avisos de mas de
 * una IncuNest (acompañar), cada fila dice de cual. Vacia: el pollito y una frase.
 * Titulo arriba (y 0..40), lista con scroll en y 40..280 y la barra debajo.
 * event_log toma un mutex (y una escritura en NVS puede tenerlo unos ms): solo se lee al cambiar,
 * al abrir la pantalla y una vez por minuto mientras se ve (los «hace N min»). */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "assets.h"
#include "event_log.h"
#include "settings.h"
#include "ui_internal.h"
#include "ui_rel_time.h"
#include "ui_toy.h"

#define TITLE_H  40
#define NOTICES_LIST_H   (UI_NAV_Y - TITLE_H)
#define BADGE_D  34
#define ROW_W    224
#define TEXT_W   (ROW_W - BADGE_D - 8)
#define TIME_VALID_EPOCH 1700000000u /* antes de esto el reloj no esta en hora */

static lv_obj_t *s_scr, *s_title, *s_list, *s_empty, *s_empty_lbl;
static event_entry_t s_entries[EVENT_LOG_MAX]; /* copia del anillo: fuera de la pila de LVGL */

/* ------------------------------------------------------------- iconos */

/* Circulo del icono, como los del tratamiento en Inicio: activo = relleno de su color con el
 * icono en blanco; el que termina, blanco con el icono atenuado. */
static lv_obj_t *badge(lv_obj_t *row, lv_color_t col, bool filled)
{
    lv_obj_t *c = lv_obj_create(row);
    lv_obj_set_size(c, BADGE_D, BADGE_D);
    toy_box(c, LV_RADIUS_CIRCLE, 0); /* circulo de borde tinta, como los iconos de Inicio */
    lv_obj_set_style_bg_color(c, filled ? col : COL_CARD, 0);
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

/* La cabeza del bebe: nina con su gorro rosa; nino o sin sexo, el azul de siempre */
static const lv_image_dsc_t *baby_head(uint8_t sex)
{
    return sex == TWIN_SEX_FEMALE ? &img_baby_head_girl : &img_baby_head_boy;
}

/* Compartido con la franja de aviso (ui_notice.c) para que icono y color coincidan. */
void ui_notice_icon(lv_obj_t *row, uint8_t kind, uint8_t sex)
{
    lv_obj_t *c;
    switch (kind) {
    case TWIN_NOTICE_ADMISSION: /* llega un bebe */
        c = badge(row, COL_CORAL_SOFT, true); /* coral suave: un bebe te llama */
        badge_image(c, baby_head(sex), false, LV_OPA_COVER, LV_SCALE_NONE);
        break;
    case TWIN_NOTICE_PARENTS:
        c = badge(row, COL_CORAL_SOFT, true);
        lv_obj_center(toy_heart_img(c, TOY_HEART_FULL, 2)); /* corazon de pixeles de 18x16 */
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
    default: /* salida: el mismo bebe en gris, neutro, sin colores de alarma */
        c = badge(row, COL_CARD, false);
        badge_image(c, baby_head(sex), false, LV_OPA_60, LV_SCALE_NONE);
        break;
    }
}

/* ------------------------------------------------------------- filas */

static void row_create(const event_entry_t *e, uint32_t now, uint8_t lang, bool show_inc)
{
    lv_obj_t *row = lv_obj_create(s_list);
    lv_obj_set_size(row, ROW_W, LV_SIZE_CONTENT);
    toy_box(row, TOY_R_CARD, TOY_SHADOW_M); /* tarjeta de juguete, como la app */
    lv_obj_set_style_pad_all(row, 6, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(row, false);
    lv_obj_set_clickable(row, false); /* el dedo arrastra la lista */
    ui_notice_icon(row, e->kind, e->sex);

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
    lv_obj_set_style_text_color(txt, COL_INK, 0);
    lv_label_set_text(txt, ui_notice_text((twin_notice_t)e->kind));

    char when[24], line[64];
    ui_rel_time(now ? e->at : 0, now, when, sizeof(when), lang); /* sin reloj en hora: «-» */
    if (show_inc && e->inc[0]) snprintf(line, sizeof(line), "%s · %s %s", when, tr(STR_INCUNEST), e->inc);
    else snprintf(line, sizeof(line), "%s", when);
    lv_obj_t *t = lv_label_create(col);
    lv_obj_set_style_text_font(t, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(t, COL_MUTED, 0);
    lv_label_set_text(t, line);
}

/* ----------------------------------------------------------------- build */

lv_obj_t *ui_notices_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_scrollable(s_scr, false);

    s_title = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_title, &lv_font_title_20, 0);
    lv_obj_set_style_text_color(s_title, COL_INK, 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 9);

    s_list = lv_obj_create(s_scr);
    lv_obj_set_size(s_list, 240, NOTICES_LIST_H);
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

    /* vacio: pollito y frase, centrados en la zona de la lista */
    s_empty = lv_obj_create(s_scr);
    lv_obj_set_size(s_empty, 240, NOTICES_LIST_H);
    lv_obj_set_pos(s_empty, 0, TITLE_H);
    lv_obj_set_style_bg_opa(s_empty, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_empty, 0, 0);
    lv_obj_set_style_pad_all(s_empty, 0, 0);
    lv_obj_set_scrollable(s_empty, false);
    lv_obj_set_clickable(s_empty, false);
    /* el pollito dentro de una pantallita pequeña, como la app */
    lv_obj_t *st = lv_obj_create(s_empty);
    lv_obj_set_size(st, 112, 92);
    lv_obj_align(st, LV_ALIGN_CENTER, 0, -36);
    toy_box(st, 8, TOY_SHADOW_M);
    lv_obj_set_style_bg_color(st, COL_STAGE, 0);
    lv_obj_set_style_pad_all(st, 0, 0);
    lv_obj_set_scrollable(st, false);
    lv_obj_set_clickable(st, false);
    lv_obj_t *chick = lv_image_create(st);
    lv_image_set_src(chick, &img_chick_64);
    lv_obj_center(chick);
    s_empty_lbl = lv_label_create(s_empty);
    lv_obj_set_width(s_empty_lbl, 200);
    lv_label_set_long_mode(s_empty_lbl, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_font(s_empty_lbl, &lv_font_title_16, 0);
    lv_obj_set_style_text_color(s_empty_lbl, COL_INK, 0);
    lv_obj_set_style_text_align(s_empty_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_empty_lbl, LV_ALIGN_CENTER, 0, 36);

    ui_notices_retranslate(); /* incluye ui_notices_refresh() */
    return s_scr;
}

void ui_notices_retranslate(void)
{
    lv_label_set_text(s_title, tr(STR_NOTICES));
    lv_label_set_text(s_empty_lbl, tr(STR_NOTICES_EMPTY));
    ui_notices_refresh(); /* los textos de las filas y los «hace N min» */
}

void ui_notices_refresh(void)
{
    uint8_t n = 0;
    uint8_t total = event_log_count();
    while (n < total && n < EVENT_LOG_MAX && event_log_get(n, &s_entries[n])) n++;
    lv_obj_set_hidden(s_empty, n > 0);
    lv_obj_set_hidden(s_list, n == 0);

    int32_t sy = lv_obj_get_scroll_y(s_list); /* que el refresco de cada minuto no te suba arriba */
    lv_obj_clean(s_list);
    if (n == 0) return;
    bool several = false; /* avisos de mas de una IncuNest: cada fila dice de cual */
    for (uint8_t i = 1; i < n && !several; i++) {
        several = strncmp(s_entries[i].inc, s_entries[0].inc, sizeof(s_entries[0].inc)) != 0;
    }
    time_t t = time(NULL);
    uint32_t now = t >= (time_t)TIME_VALID_EPOCH ? (uint32_t)t : 0;
    uint8_t lang = settings_lang();
    for (uint8_t i = 0; i < n; i++) row_create(&s_entries[i], now, lang, several);
    lv_obj_update_layout(s_list);
    lv_obj_scroll_to_y(s_list, sy, LV_ANIM_OFF);
}
