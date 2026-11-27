/* Pantalla de Historial: los bebes que han pasado por tus IncuNest (stays), en dos niveles.
 *   1. Tus IncuNest, la ultima con cambios primero: la IncuNest pequeña con el numero de bebes que
 *      han pasado por ella, «IncuNest 297» y la bandera con el pais. Se toca para entrar.
 *   2. Los bebes de esa IncuNest, del mas reciente al mas antiguo: su cabeza (nino o nina), cuando
 *      llego y cuando salio (o «Ahora en la IncuNest»). «‹» arriba a la izquierda vuelve.
 * Los cambios de estado de cada momento estan en Notificaciones (ui_notices.c).
 * Titulo arriba (y 0..40), lista con scroll en y 40..280 y la barra debajo. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "assets.h"
#include "baby_name.h"
#include "settings.h"
#include "stays.h"
#include "ui_internal.h"
#include "ui_toy.h"
#include "ui_rel_time.h"

#define TITLE_H  40
#define HIST_LIST_H   (UI_NAV_Y - TITLE_H)
#define ROW_W    224
#define SLOT_W   40  /* icono de la izquierda (IncuNest con su numero, o la cabeza del bebe) */
#define TEXT_W   (ROW_W - 12 - SLOT_W - 8 - 20) /* y la flecha de la derecha */
#define TIME_VALID_EPOCH 1700000000u /* antes de esto el reloj no esta en hora */

static lv_obj_t *s_scr, *s_title, *s_back, *s_list, *s_empty, *s_empty_lbl;
static stay_log_t s_log; /* copia de stays: fuera de la pila de LVGL */
static char s_sel[TWIN_INCUBATOR_ID_LEN];     /* IncuNest abierta; "" = lista de IncuNest */
static char s_current[TWIN_INCUBATOR_ID_LEN]; /* la IncuNest del panel ahora */

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

static lv_obj_t *text(lv_obj_t *parent, const char *txt, const lv_font_t *font, lv_color_t col)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_set_style_max_width(l, TEXT_W, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_text(l, txt);
    return l;
}

/* Tarjeta blanca en fila: icono a la izquierda y columna de textos */
static lv_obj_t *card(lv_obj_t **col_out)
{
    lv_obj_t *c = box(s_list, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(c, ROW_W, LV_SIZE_CONTENT);
    toy_box(c, TOY_R_CARD, TOY_SHADOW_M); /* tarjeta de juguete, como la app */
    lv_obj_set_style_pad_all(c, 6, 0);
    lv_obj_set_style_pad_column(c, 8, 0);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *slot = box(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(slot, SLOT_W, 34);
    lv_obj_set_layout(slot, LV_LAYOUT_NONE);
    *col_out = box(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(*col_out, TEXT_W, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(*col_out, 1, 0);
    return c;
}

static lv_obj_t *card_slot(lv_obj_t *c) { return lv_obj_get_child(c, 0); }

static void show_list(void);

/* La lista se rehace fuera del evento: la tarjeta tocada se borra con ella */
static void enter_async(void *arg)
{
    (void)arg;
    show_list();
    lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);
}

static void inc_clicked(lv_event_t *e)
{
    const char *inc = lv_event_get_user_data(e);
    snprintf(s_sel, sizeof(s_sel), "%s", inc);
    lv_async_call(enter_async, NULL);
}

static void back_clicked(lv_event_t *e)
{
    (void)e;
    s_sel[0] = '\0';
    lv_async_call(enter_async, NULL);
}

/* Nivel 1: una IncuNest con el numero de bebes en la esquina de su dibujo */
static void incubator_row(const stay_inc_t *inc)
{
    lv_obj_t *col;
    lv_obj_t *c = card(&col);
    lv_obj_set_clickable(c, true);
    lv_obj_add_flag(c, UI_FLAG_CLICK_SOUND);
    lv_obj_add_event_cb(c, inc_clicked, LV_EVENT_SHORT_CLICKED, (void *)inc->inc); /* vive en s_log */

    lv_obj_t *slot = card_slot(c);
    lv_obj_t *nest = lv_image_create(slot);
    lv_image_set_src(nest, &img_incunest_28);
    lv_obj_set_pos(nest, 0, 6);
    lv_obj_t *num = lv_label_create(slot);
    lv_label_set_text_fmt(num, "%u", (unsigned)inc->babies);
    lv_obj_set_style_text_font(num, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(num, lv_color_white(), 0);
    lv_obj_set_style_text_align(num, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(num, COL_CORAL, 0); /* numero de bebes: insignia coral, como la app */
    lv_obj_set_style_bg_opa(num, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(num, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(num, COL_INK, 0);
    lv_obj_set_style_border_width(num, 2, 0);
    lv_obj_set_style_pad_hor(num, 4, 0);
    lv_obj_set_style_pad_ver(num, 1, 0);
    lv_obj_set_style_min_width(num, 20, 0);
    lv_obj_align(num, LV_ALIGN_TOP_RIGHT, 0, -2);

    char title[8 + TWIN_INCUBATOR_ID_LEN + 2];
    snprintf(title, sizeof(title), "%s %s", tr(STR_INCUNEST), inc->inc);
    text(col, title, &lv_font_title_16, COL_INK);
    if (inc->country[0]) {
        lv_obj_t *line = box(col, LV_FLEX_FLOW_ROW);
        lv_obj_set_size(line, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_column(line, 5, 0);
        lv_obj_set_flex_align(line, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        const lv_image_dsc_t *flag = flag_image(inc->country);
        if (flag) {
            lv_obj_t *f = lv_image_create(line);
            lv_image_set_src(f, flag);
            lv_image_set_inner_align(f, LV_IMAGE_ALIGN_STRETCH);
            lv_obj_set_size(f, (int32_t)flag->header.w * 12 / flag->header.h, 12);
        }
        text(line, tr_country(inc->country), &lv_font_es_12, COL_MUTED);
    }
    lv_obj_t *chev = lv_label_create(c);
    lv_label_set_text(chev, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(chev, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(chev, COL_INK, 0);
}

/* Nivel 2: un bebe de la IncuNest abierta */
static void baby_row(const stay_t *b, uint32_t now, uint8_t lang)
{
    lv_obj_t *col;
    lv_obj_t *c = card(&col);
    bool here = b->open && strncmp(b->inc, s_current, sizeof(s_current)) == 0; /* esta ahora */
    lv_obj_t *head = lv_image_create(card_slot(c));
    lv_image_set_src(head, b->sex == TWIN_SEX_FEMALE ? &img_baby_head_girl : &img_baby_head_boy);
    lv_obj_center(head);
    lv_obj_set_style_image_opa(head, b->open ? LV_OPA_COVER : LV_OPA_70, 0);

    /* el nombre que le has puesto, solo para el bebe que esta ahora (es el de la estancia actual) */
    const char *name = here ? baby_name_local() : "";
    text(col, name[0] ? name : tr(STR_HIST_BABY), &lv_font_title_16, COL_INK);
    char when[24], line[48];
    if (b->arrived_at) {
        ui_rel_time(now ? b->arrived_at : 0, now, when, sizeof(when), lang);
        snprintf(line, sizeof(line), tr(STR_HIST_ARRIVED), when);
        text(col, line, &lv_font_es_12, COL_MUTED);
    }
    if (here) {
        text(col, tr(STR_HIST_NOW), &lv_font_es_12, COL_CORAL_INK);
    } else if (!b->open && b->left_at) {
        ui_rel_time(now ? b->left_at : 0, now, when, sizeof(when), lang);
        snprintf(line, sizeof(line), tr(STR_HIST_LEFT), when);
        text(col, line, &lv_font_es_12, COL_MUTED);
    }
}

/* Pinta el nivel que toque con lo que hay en s_log */
static void show_list(void)
{
    const stay_inc_t *sel = NULL;
    for (int i = 0; s_sel[0] && i < STAY_INC_MAX; i++) {
        if (strncmp(s_log.inc[i].inc, s_sel, sizeof(s_sel)) == 0) sel = &s_log.inc[i];
    }
    if (!sel) s_sel[0] = '\0'; /* ya no esta (se piso): a la lista */
    lv_obj_set_hidden(s_back, sel == NULL);
    if (sel) {
        char title[8 + TWIN_INCUBATOR_ID_LEN + 2];
        snprintf(title, sizeof(title), "%s %s", tr(STR_INCUNEST), sel->inc);
        lv_label_set_text(s_title, title);
    } else {
        lv_label_set_text(s_title, tr(STR_HISTORY));
    }

    lv_obj_clean(s_list);
    time_t t = time(NULL);
    uint32_t now = t >= (time_t)TIME_VALID_EPOCH ? (uint32_t)t : 0;
    uint8_t lang = settings_lang();
    uint8_t n;
    if (sel) {
        static const stay_t *babies[STAY_LOG_MAX];
        n = stay_log_babies(&s_log, sel->inc, babies, STAY_LOG_MAX);
        for (uint8_t i = 0; i < n; i++) baby_row(babies[i], now, lang);
        if (sel->babies > n) { /* los mas antiguos ya no estan guardados: al menos se cuentan */
            char more[48];
            snprintf(more, sizeof(more), tr(STR_HIST_MORE), (unsigned)(sel->babies - n));
            lv_obj_t *l = lv_label_create(s_list);
            lv_obj_set_width(l, ROW_W);
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_font(l, &lv_font_es_12, 0);
            lv_obj_set_style_text_color(l, COL_MUTED, 0);
            lv_label_set_text(l, more);
        }
    } else {
        uint8_t idx[STAY_INC_MAX];
        n = stay_log_incubators(&s_log, idx);
        for (uint8_t i = 0; i < n; i++) incubator_row(&s_log.inc[idx[i]]);
    }
    lv_obj_set_hidden(s_empty, n > 0);
    lv_obj_set_hidden(s_list, n == 0);
}

/* ----------------------------------------------------------------- build */

lv_obj_t *ui_history_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_scrollable(s_scr, false);

    s_title = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_title, &lv_font_title_20, 0);
    lv_obj_set_style_text_color(s_title, COL_INK, 0);
    lv_obj_set_style_max_width(s_title, 168, 0);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 9);

    s_back = lv_button_create(s_scr);
    lv_obj_set_size(s_back, 30, 30); /* atras: chevron en un circulo de juguete, como la app */
    lv_obj_set_pos(s_back, 7, 5);
    toy_box(s_back, LV_RADIUS_CIRCLE, TOY_SHADOW_S);
    lv_obj_set_style_pad_all(s_back, 0, 0);
    lv_obj_set_ext_click_area(s_back, 8);
    lv_obj_add_event_cb(s_back, back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *arrow = lv_label_create(s_back);
    lv_label_set_text(arrow, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(arrow, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(arrow, COL_INK, 0);
    lv_obj_center(arrow);
    lv_obj_set_hidden(s_back, true);

    s_list = lv_obj_create(s_scr);
    lv_obj_set_size(s_list, 240, HIST_LIST_H);
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

    /* vacio: la IncuNest y una frase, centradas en la zona de la lista */
    s_empty = lv_obj_create(s_scr);
    lv_obj_set_size(s_empty, 240, HIST_LIST_H);
    lv_obj_set_pos(s_empty, 0, TITLE_H);
    lv_obj_set_style_bg_opa(s_empty, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_empty, 0, 0);
    lv_obj_set_style_pad_all(s_empty, 0, 0);
    lv_obj_set_scrollable(s_empty, false);
    lv_obj_set_clickable(s_empty, false);
    /* la IncuNest dentro de una pantallita pequeña, como la app */
    lv_obj_t *st = lv_obj_create(s_empty);
    lv_obj_set_size(st, 112, 92);
    lv_obj_align(st, LV_ALIGN_CENTER, 0, -36);
    toy_box(st, 8, TOY_SHADOW_M);
    lv_obj_set_style_bg_color(st, COL_STAGE, 0);
    lv_obj_set_style_pad_all(st, 0, 0);
    lv_obj_set_scrollable(st, false);
    lv_obj_set_clickable(st, false);
    lv_obj_t *nest = lv_image_create(st);
    lv_image_set_src(nest, &img_incunest_map);
    lv_obj_center(nest);
    s_empty_lbl = lv_label_create(s_empty);
    lv_obj_set_width(s_empty_lbl, 200);
    lv_label_set_long_mode(s_empty_lbl, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_font(s_empty_lbl, &lv_font_title_16, 0);
    lv_obj_set_style_text_color(s_empty_lbl, COL_INK, 0);
    lv_obj_set_style_text_align(s_empty_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_empty_lbl, LV_ALIGN_CENTER, 0, 36);

    ui_history_retranslate(); /* incluye ui_history_refresh() */
    return s_scr;
}

void ui_history_retranslate(void)
{
    lv_label_set_text(s_empty_lbl, tr(STR_HISTORY_EMPTY));
    ui_history_refresh();
}

void ui_history_refresh(void)
{
    int32_t sy = lv_obj_get_scroll_y(s_list); /* que el refresco de cada minuto no te suba arriba */
    stays_copy(&s_log);
    show_list();
    lv_obj_update_layout(s_list);
    lv_obj_scroll_to_y(s_list, sy, LV_ANIM_OFF);
}

void ui_history_reset(void)
{
    if (!s_sel[0]) return;
    s_sel[0] = '\0';
    show_list();
    lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);
}

void ui_history_apply(const twin_snapshot_t *s)
{
    const char *inc = s->link.paired && !s->demo ? s->link.incubator_id : "";
    if (strncmp(inc, s_current, sizeof(s_current)) == 0) return;
    snprintf(s_current, sizeof(s_current), "%s", inc);
    if (s_sel[0]) show_list(); /* cambia quien esta «ahora» */
}
