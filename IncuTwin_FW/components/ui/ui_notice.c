/* Franja de aviso en pantalla (spec hmi-navegacion-avisos, "Aviso en pantalla"): baja desde arriba
 * en la capa superior de LVGL con el icono y el texto del aviso, se queda 4 s y sube. Si llegan
 * varios a la vez salen uno detras de otro. Tocarla abre el Historial y vacia la cola. No sale en
 * el splash, el onboarding, la despedida ni con el aviso o el teclado del nombre abiertos: se
 * encola y sale al cerrarse. Todo en la tarea de LVGL. */
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

static lv_obj_t *s_bar, *s_lbl;
static lv_timer_t *s_hold_timer;
static notice_state_t s_state = N_IDLE;
static uint8_t s_queue[QUEUE_LEN]; /* bit de twin_notice_t (0..6) de cada aviso pendiente */
static uint8_t s_count;
static bool s_blocked_logged;

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

static void show(uint8_t kind)
{
    /* el icono se rehace: es el mismo del Historial (ui_notice_icon) */
    lv_obj_clean(s_bar);
    ui_notice_icon(s_bar, kind);
    s_lbl = lv_label_create(s_bar);
    lv_obj_set_size(s_lbl, BANNER_W - 34 - 8 - 16, 34);
    lv_label_set_long_mode(s_lbl, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_font(s_lbl, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_lbl, COL_NAVY_DARK, 0);
    lv_label_set_text(s_lbl, ui_notice_text((twin_notice_t)(1u << kind)));
    ESP_LOGI(TAG, "franja: \"%s\" (quedan %u)", lv_label_get_text(s_lbl), (unsigned)s_count);
    lv_obj_set_y(s_bar, Y_HIDDEN);
    lv_obj_set_hidden(s_bar, false);
    s_state = N_IN;
    slide(Y_HIDDEN, Y_SHOWN, in_done_cb);
}

static void bar_clicked_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "franja tocada: Historial, cola vaciada (%u)", (unsigned)s_count);
    s_count = 0;
    lv_anim_delete(s_bar, y_cb);
    if (s_hold_timer) {
        lv_timer_delete(s_hold_timer);
        s_hold_timer = NULL;
    }
    lv_obj_set_hidden(s_bar, true);
    s_state = N_IDLE;
    ui_go(UI_TAB_HISTORY);
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
        if (c && c != s_bar && !lv_obj_is_hidden(c)) return true;
    }
    return false;
}

void ui_notice_push(uint32_t mask)
{
    for (uint8_t bit = 0; bit < 32; bit++) {
        if (!(mask & (1u << bit))) continue;
        if (s_count >= QUEUE_LEN) { /* mas de 8 sin ver: se pierde el mas antiguo (sigue en el Historial) */
            for (uint8_t i = 1; i < QUEUE_LEN; i++) s_queue[i - 1] = s_queue[i];
            s_count--;
        }
        s_queue[s_count++] = bit;
    }
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
    s_blocked_logged = false;
    uint8_t kind = s_queue[0];
    for (uint8_t i = 1; i < s_count; i++) s_queue[i - 1] = s_queue[i];
    s_count--;
    show(kind);
}
