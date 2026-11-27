/* Franja de aviso en pantalla (spec hmi-navegacion-avisos, "Aviso en pantalla"): baja desde arriba
 * en la capa superior de LVGL con el icono y el texto del aviso, se queda 4 s y sube. Si llegan
 * varios a la vez salen uno detras de otro. Dos clases de aviso en la misma cola:
 *   - lo que vive el bebe (twin_notice_t): tocarla abre Notificaciones y vacia la cola;
 *   - recompensas (sello nuevo, subida de nivel, guirnalda; spec rewards): tocarla abre Corazones.
 *     No se guardan en Notificaciones.
 *   - texto de la IncuPray (vispera, una hora antes, inicio, resumen): tocar la del inicio entra.
 *   - la tarjeta de subida de nivel (Caricia y Cosquillitas abren algo; Abrazo y Achuchon, la franja):
 *     centrada, se queda hasta tocarla; espera en la misma cola y bajo las mismas condiciones.
 * No sale en el splash, el onboarding, la despedida ni con el aviso o el teclado del nombre
 * abiertos: se encola y sale al cerrarse. Todo en la tarea de LVGL. */
#include <stdio.h>

#include "assets.h"
#include "esp_log.h"
#include "ui_internal.h"
#include "ui_name.h"

static const char *TAG = "aviso";

#define BANNER_W   224
#define BANNER_H   44
#define BANNER_X   8
#define Y_HIDDEN   (-BANNER_H - 4)
#define Y_SHOWN    4
#define SLIDE_MS   200
#define HOLD_MS    4000
#define QUEUE_LEN  8

typedef enum { N_IDLE, N_IN, N_HOLD, N_OUT } notice_state_t;
typedef enum { Q_NOTICE = 0, Q_SEAL, Q_LEVEL, Q_GARLAND, Q_TEXT, Q_CARD } q_type_t;
#define Q_TEXT_LEN 64

typedef struct {
    uint8_t type; /* q_type_t */
    uint8_t arg;  /* bit de twin_notice_t (0..6), rw_seal_t o rw_level_t */
    uint8_t sex;  /* twin_sex_t del bebe (avisos del bebe); Q_CARD: 1 = aviso pendiente (catchup) */
    char txt[Q_TEXT_LEN]; /* Q_TEXT: el texto; arg = 1 si al tocarla se entra en la IncuPray */
} q_item_t;

static lv_obj_t *s_bar, *s_lbl;
static lv_timer_t *s_hold_timer;
static notice_state_t s_state = N_IDLE;
static q_item_t s_queue[QUEUE_LEN];
static uint8_t s_count;
static bool s_blocked_logged;
static uint8_t s_shown_type; /* q_type_t de la que se ve (a donde lleva el toque) */
static uint8_t s_shown_arg;

static void y_cb(void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, v); }

static void slide(int32_t from, int32_t to, lv_anim_completed_cb_t done)
{
    lv_anim_delete(s_bar, y_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_bar);
    lv_anim_set_exec_cb(&a, y_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, SLIDE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, done);
    lv_anim_start(&a);
}

static void out_done_cb(lv_anim_t *a)
{
    (void)a;
    lv_obj_set_hidden(s_bar, true);
    s_state = N_IDLE; /* el siguiente lo saca ui_notice_poll() */
}

static void hold_done_cb(lv_timer_t *t)
{
    (void)t; /* una sola vez: LVGL borra el timer */
    s_hold_timer = NULL;
    s_state = N_OUT;
    slide(Y_SHOWN, Y_HIDDEN, out_done_cb);
}

static void in_done_cb(lv_anim_t *a)
{
    (void)a;
    s_state = N_HOLD;
    s_hold_timer = lv_timer_create(hold_done_cb, HOLD_MS, NULL);
    lv_timer_set_repeat_count(s_hold_timer, 1);
}

/* Icono de una recompensa en el sitio del icono del aviso (34 px) */
static void reward_icon(const q_item_t *q)
{
    if (q->type == Q_SEAL) {
        ui_seal_icon(s_bar, (rw_seal_t)q->arg, true, 34);
        return;
    }
    lv_obj_t *c = lv_obj_create(s_bar);
    lv_obj_set_size(c, 34, 34);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c, COL_CARD, 0);
    lv_obj_set_style_border_width(c, 2, 0);
    lv_obj_set_style_border_color(c, q->type == Q_GARLAND ? COL_OK : q->type == Q_TEXT ? COL_NAVY : COL_CORAL, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_set_scrollable(c, false);
    lv_obj_set_clickable(c, false);
    lv_obj_t *i = lv_image_create(c);
    lv_image_set_src(i, &img_heart);
    lv_image_set_scale(i, 180);
    lv_obj_center(i);
}

static void enqueue(q_item_t q);

/* ------------------------------------------------------- tarjeta de nivel */

static void card_close_cb(lv_event_t *e)
{
    lv_obj_delete_async(lv_event_get_user_data(e)); /* el fondo y la tarjeta; no se borra dentro de su evento */
}

static lv_obj_t *card_label(lv_obj_t *card, const char *t, const lv_font_t *f, lv_color_t col)
{
    lv_obj_t *l = lv_label_create(card);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}

/* Fondo que tapa toda la capa superior (modal: cuenta como capa ocupada) y la tarjeta en medio */
static void card_show(rw_level_t level, bool catchup)
{
    lv_obj_t *back = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(back, COL_NAVY_DARK, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_40, 0);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *c = lv_obj_create(back);
    lv_obj_set_size(c, 208, LV_SIZE_CONTENT);
    lv_obj_center(c);
    lv_obj_set_style_bg_color(c, COL_CARD, 0);
    lv_obj_set_style_radius(c, 14, 0);
    lv_obj_set_style_border_width(c, 2, 0);
    lv_obj_set_style_border_color(c, COL_CORAL, 0);
    lv_obj_set_style_pad_all(c, 12, 0);
    lv_obj_set_style_pad_row(c, 8, 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(c, false);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c, card_close_cb, LV_EVENT_CLICKED, back);

    char title[48];
    if (catchup) snprintf(title, sizeof(title), "%s", tr(STR_AFF_CATCHUP));
    else snprintf(title, sizeof(title), tr(STR_RW_LEVEL_UP), ui_level_name(level));
    card_label(c, title, &lv_font_es_16, COL_NAVY);
    /* el aviso pendiente de quien ya esta en Cosquillitas o mas enseña las dos cosas abiertas */
    if (catchup && level >= RW_LEVEL_TICKLES) card_label(c, tr(STR_AFF_OPEN_CARESS), &lv_font_es_14, COL_NAVY_DARK);
    card_label(c, tr(level >= RW_LEVEL_TICKLES ? STR_AFF_OPEN_TICKLES : STR_AFF_OPEN_CARESS), &lv_font_es_14,
               COL_NAVY_DARK);

    lv_obj_t *btn = lv_button_create(c);
    lv_obj_set_style_bg_color(btn, COL_CORAL, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_add_event_cb(btn, card_close_cb, LV_EVENT_CLICKED, back);
    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, tr(STR_AFF_OK));
    lv_obj_set_style_text_font(bl, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(bl, lv_color_white(), 0);
    ESP_LOGI(TAG, "tarjeta de nivel: \"%s\"", title);
}

void ui_level_card_show(rw_level_t level, bool catchup)
{
    enqueue((q_item_t){ .type = Q_CARD, .arg = (uint8_t)level, .sex = catchup ? 1 : 0 });
}

static void show(const q_item_t *q)
{
    /* el icono se rehace: el de los avisos del bebe es el mismo de Notificaciones (ui_notice_icon,
     * que quiere el bit ya desplazado: con el indice, un ingreso salia con el icono de la salida) */
    lv_obj_clean(s_bar);
    if (q->type == Q_NOTICE) ui_notice_icon(s_bar, (uint8_t)(1u << q->arg), q->sex);
    else reward_icon(q);
    s_lbl = lv_label_create(s_bar);
    lv_obj_set_size(s_lbl, BANNER_W - 34 - 8 - 16, 34);
    lv_label_set_long_mode(s_lbl, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_font(s_lbl, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_lbl, COL_NAVY_DARK, 0);
    switch (q->type) {
    case Q_SEAL:    lv_label_set_text_fmt(s_lbl, tr(STR_RW_NEW_SEAL), ui_seal_name((rw_seal_t)q->arg)); break;
    case Q_LEVEL:   lv_label_set_text_fmt(s_lbl, tr(STR_RW_LEVEL_UP), ui_level_name((rw_level_t)q->arg)); break;
    case Q_GARLAND: lv_label_set_text(s_lbl, tr(STR_RW_GARLAND)); break;
    case Q_TEXT:    lv_label_set_text(s_lbl, q->txt); break;
    default:        lv_label_set_text(s_lbl, ui_notice_text((twin_notice_t)(1u << q->arg))); break;
    }
    s_shown_type = q->type;
    s_shown_arg = q->arg;
    ESP_LOGI(TAG, "franja: \"%s\" (quedan %u)", lv_label_get_text(s_lbl), (unsigned)s_count);
    lv_obj_set_y(s_bar, Y_HIDDEN);
    lv_obj_set_hidden(s_bar, false);
    s_state = N_IN;
    slide(Y_HIDDEN, Y_SHOWN, in_done_cb);
}

static void bar_clicked_cb(lv_event_t *e)
{
    (void)e;
    bool reward = s_shown_type != Q_NOTICE && s_shown_type != Q_TEXT;
    ESP_LOGI(TAG, "franja tocada: %s, cola vaciada (%u)",
             s_shown_type == Q_TEXT ? "IncuPray" : reward ? "Corazones" : "Notificaciones", (unsigned)s_count);
    s_count = 0;
    lv_anim_delete(s_bar, y_cb);
    if (s_hold_timer) {
        lv_timer_delete(s_hold_timer);
        s_hold_timer = NULL;
    }
    lv_obj_set_hidden(s_bar, true);
    s_state = N_IDLE;
    if (s_shown_type == Q_TEXT) {
        ui_go(UI_TAB_HOME);
        if (s_shown_arg) ui_incupray_open();
    } else if (reward) {
        ui_go(UI_TAB_HEARTS);
        ui_hearts_show_top();
    } else {
        ui_go(UI_TAB_NOTICES);
    }
}

void ui_notice_init(void)
{
    s_bar = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_bar, BANNER_W, BANNER_H);
    lv_obj_set_pos(s_bar, BANNER_X, Y_HIDDEN);
    lv_obj_set_style_bg_color(s_bar, COL_CARD, 0);
    lv_obj_set_style_radius(s_bar, 12, 0);
    lv_obj_set_style_border_width(s_bar, 1, 0);
    lv_obj_set_style_border_color(s_bar, COL_OFFLINE, 0);
    lv_obj_set_style_pad_all(s_bar, 5, 0);
    lv_obj_set_style_pad_column(s_bar, 8, 0);
    lv_obj_set_flex_flow(s_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(s_bar, false);
    lv_obj_set_clickable(s_bar, true);
    lv_obj_add_event_cb(s_bar, bar_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_hidden(s_bar, true);
}

/* true si hay en la capa superior algun objeto visible que no sea la franja (dialogos, msgbox...) */
bool ui_notice_layer_busy(void)
{
    lv_obj_t *top = lv_layer_top();
    uint32_t n = lv_obj_get_child_count(top);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(top, i);
        if (c && c != s_bar && c != ui_nav_obj() && !lv_obj_is_hidden(c)) return true;
    }
    return false;
}

static void enqueue(q_item_t q)
{
    if (s_count >= QUEUE_LEN) { /* mas de 8 sin ver: se pierde el mas antiguo que no sea una tarjeta de
                                 * nivel (los avisos siguen en Notificaciones; los sellos, en Logros; la
                                 * tarjeta no tiene otro sitio donde verse). Si todo son tarjetas, se
                                 * pierde la mas antigua si entra otra tarjeta y la nueva si no lo es. */
        uint8_t drop = QUEUE_LEN;
        for (uint8_t i = 0; i < QUEUE_LEN; i++) {
            if (s_queue[i].type != Q_CARD) { drop = i; break; }
        }
        if (drop == QUEUE_LEN) {
            if (q.type != Q_CARD) return;
            drop = 0;
        }
        for (uint8_t i = drop + 1; i < QUEUE_LEN; i++) s_queue[i - 1] = s_queue[i];
        s_count--;
    }
    s_queue[s_count++] = q;
}

void ui_notice_push(uint32_t mask, uint8_t sex)
{
    for (uint8_t bit = 0; bit < 32; bit++) {
        if (mask & (1u << bit)) enqueue((q_item_t){ .type = Q_NOTICE, .arg = bit, .sex = sex });
    }
}

void ui_notice_push_reward(uint8_t new_seals, bool level_up, uint8_t level, bool garland)
{
    for (uint8_t i = 0; i < RW_SEAL_COUNT; i++) {
        if (new_seals & (1u << i)) enqueue((q_item_t){ .type = Q_SEAL, .arg = i });
    }
    if (level_up) {
        if (level == RW_LEVEL_CARESS || level == RW_LEVEL_TICKLES) ui_level_card_show((rw_level_t)level, false);
        else enqueue((q_item_t){ .type = Q_LEVEL, .arg = level }); /* Abrazo y Achuchon: aun sin nada que abrir */
    }
    if (garland) enqueue((q_item_t){ .type = Q_GARLAND });
}

void ui_notice_push_text(const char *txt, bool opens_incupray)
{
    q_item_t q = { .type = Q_TEXT, .arg = opens_incupray ? 1 : 0 };
    snprintf(q.txt, sizeof(q.txt), "%s", txt ? txt : "");
    enqueue(q);
}

void ui_notice_poll(bool blocked)
{
    if (s_state != N_IDLE || s_count == 0) {
        return;
    }
    if (blocked) {
        if (!s_blocked_logged) {
            s_blocked_logged = true;
            ESP_LOGI(TAG, "%u aviso(s) en cola: pantalla ocupada", (unsigned)s_count);
        }
        return;
    }
    /* la tarjeta es modal: no tapa un «Coge mi mano» en marcha (la franja si puede salir con el dedo
     * puesto; la tarjeta espera a HH_IDLE, tras el agradecimiento) */
    if (s_queue[0].type == Q_CARD && !ui_home_gesture_idle()) return;
    s_blocked_logged = false;
    q_item_t q = s_queue[0];
    for (uint8_t i = 1; i < s_count; i++) s_queue[i - 1] = s_queue[i];
    s_count--;
    if (q.type == Q_CARD) card_show((rw_level_t)q.arg, q.sex != 0); /* no usa la franja: s_state sigue en IDLE */
    else show(&q);
}
