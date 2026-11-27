/* Ventana de la actualizacion (spec ota-update, «Ventana de la actualización»): cuando hay una OTA
 * lista, una ventana modal en la capa superior con el titulo, «No la desenchufes», una barra de
 * progreso y el porcentaje; al terminar, «Reiniciando...» y el panel reinicia solo (ota_update).
 * Cuando sale:
 *   - en seguida si el panel acaba de arrancar y nadie ha tocado la pantalla (5 min);
 *   - si no, tras 2 min sin tocar la pantalla y sin «Coge mi mano» a medias;
 *   - si en 6 h no se ha dado, en cuanto no haya gesto a medias.
 * Si la descarga falla: «No se ha podido actualizar», 4 s, y todo sigue como estaba.
 * Todo en la tarea de LVGL (ui_ota_poll desde el refresco de ui.c). */
#include "assets.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ota_update.h"
#include "ui_internal.h"
#include "ui_toy.h"

static const char *TAG = "ui_ota";

#define IDLE_MS         120000u                 /* 2 min sin tocar */
#define BOOT_WINDOW_US  (5LL * 60 * 1000000)    /* «recien arrancado» */
#define LONG_WAIT_MS    (6u * 3600u * 1000u)
#define FAILED_SHOW_MS  4000u

typedef enum { OTA_UI_IDLE = 0, OTA_UI_WAITING, OTA_UI_SHOWN, OTA_UI_FAILED } ota_ui_t;

static ota_ui_t s_state;
static bool s_touched;      /* se ha tocado la pantalla desde que arranco */
static uint32_t s_since;    /* lv_tick al saber que habia OTA */
static uint32_t s_failed_at;
static lv_obj_t *s_back, *s_bar, *s_pct, *s_text;

void ui_ota_note_touch(void) { s_touched = true; }

static void close_window(void)
{
    if (s_back) lv_obj_delete(s_back);
    s_back = s_bar = s_pct = s_text = NULL;
}

/* Fondo oscurecido a toda pantalla (tapa los toques) y tarjeta de 208 px centrada */
static void open_window(void)
{
    s_back = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_back);
    lv_obj_set_size(s_back, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_back, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_back, LV_OPA_50, 0);
    lv_obj_set_clickable(s_back, true);
    lv_obj_set_scrollable(s_back, false);

    lv_obj_t *card = lv_obj_create(s_back);
    lv_obj_set_size(card, 208, LV_SIZE_CONTENT);
    lv_obj_center(card);
    toy_box(card, 16, 4); /* como los dialogos */
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(card, false);

    lv_obj_t *t = lv_label_create(card);
    lv_label_set_text(t, tr(STR_OTA_UPDATING));
    lv_obj_set_width(t, LV_PCT(100));
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(t, &lv_font_title_16, 0);
    lv_obj_set_style_text_color(t, COL_INK, 0);

    s_text = lv_label_create(card);
    lv_label_set_text(s_text, tr(STR_OTA_DONT_UNPLUG));
    lv_obj_set_width(s_text, LV_PCT(100));
    lv_label_set_long_mode(s_text, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(s_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_text, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_text, COL_NAVY_DARK, 0);

    s_bar = lv_bar_create(card);
    lv_obj_set_size(s_bar, LV_PCT(100), 12);
    lv_bar_set_range(s_bar, 0, 100);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar, COL_CORAL_SOFT, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_bar, COL_INK, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bar, TOY_BORDER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar, COL_CORAL, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar, 0, LV_PART_INDICATOR);

    s_pct = lv_label_create(card);
    lv_label_set_text(s_pct, "0 %");
    lv_obj_set_style_text_font(s_pct, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_pct, COL_MUTED, 0);
}

static void show(const char *why)
{
    ESP_LOGW(TAG, "ventana de la actualizacion (%s)", why);
    open_window();
    s_state = OTA_UI_SHOWN;
    ota_update_go();
}

void ui_ota_poll(void)
{
    switch (s_state) {
    case OTA_UI_IDLE:
        if (!ota_update_pending()) return;
        s_state = OTA_UI_WAITING;
        s_since = lv_tick_get();
        ESP_LOGI(TAG, "OTA pendiente: esperando el momento");
        /* fall through */
    case OTA_UI_WAITING:
        if (!ota_update_pending()) { /* otra cosa la ha lanzado o cancelado */
            s_state = OTA_UI_IDLE;
            return;
        }
        if (!s_touched && esp_timer_get_time() < BOOT_WINDOW_US) {
            show("recien arrancado");
        } else if (ui_home_gesture_idle() && lv_display_get_inactive_time(NULL) >= IDLE_MS) {
            show("2 min sin tocar");
        } else if (ui_home_gesture_idle() && lv_tick_elaps(s_since) >= LONG_WAIT_MS) {
            show("6 h esperando");
        }
        return;
    case OTA_UI_SHOWN: {
        int p = ota_update_progress();
        if (p == OTA_PROGRESS_FAILED) {
            lv_label_set_text(s_text, tr(STR_OTA_FAILED));
            lv_obj_set_hidden(s_bar, true);
            lv_obj_set_hidden(s_pct, true);
            s_state = OTA_UI_FAILED;
            s_failed_at = lv_tick_get();
            return;
        }
        if (p < 0) return;
        lv_bar_set_value(s_bar, p, LV_ANIM_OFF);
        lv_label_set_text_fmt(s_pct, "%d %%", p);
        if (p >= 100) lv_label_set_text(s_text, tr(STR_OTA_RESTARTING));
        return;
    }
    case OTA_UI_FAILED:
        if (lv_tick_elaps(s_failed_at) >= FAILED_SHOW_MS) {
            close_window();
            s_state = OTA_UI_IDLE;
        }
        return;
    }
}
