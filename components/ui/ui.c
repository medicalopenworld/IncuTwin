#include "ui.h"

#include <string.h>

#include "assets.h"
#include "baby_widget.h"
#include "board.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sound.h"
#include "twin_model.h"
#include "ui_internal.h"

static const char *TAG = "ui";

#define SPLASH_MS 2500

static lv_obj_t *s_scr_splash, *s_scr_home, *s_scr_settings;

/* dato pendiente de aplicar (lo escribe el bus de eventos, lo lee la tarea LVGL) */
static SemaphoreHandle_t s_mutex;
static twin_snapshot_t s_pending;
static bool s_state_dirty;
static app_evt_settings_t s_settings_pending;
static bool s_settings_dirty;

/* ------------------------------------------------------------- navigation */

void ui_go_settings(void)
{
    lv_screen_load_anim(s_scr_settings, LV_SCR_LOAD_ANIM_MOVE_LEFT, 220, 0, false);
}

void ui_go_home(void)
{
    lv_screen_load_anim(s_scr_home, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 220, 0, false);
}

/* ----------------------------------------------------------------- dialogs */

lv_obj_t *ui_msgbox(const char *title, const char *text, const char *btn_text, lv_color_t btn_color,
                    lv_event_cb_t btn_cb, void *user_data)
{
    lv_obj_t *mb = lv_msgbox_create(NULL);
    lv_obj_set_width(mb, 208);
    lv_obj_set_style_radius(mb, 14, 0);
    lv_obj_set_style_text_font(mb, &lv_font_es_14, 0); /* heredada por titulo, texto y botones */
    lv_obj_t *t = lv_msgbox_add_title(mb, title);
    lv_obj_set_style_text_font(t, &lv_font_es_16, 0);
    lv_obj_set_style_text_color(t, COL_NAVY, 0);
    lv_obj_t *txt = lv_msgbox_add_text(mb, text);
    lv_obj_set_style_text_color(txt, COL_NAVY_DARK, 0);
    /* los botones del msgbox no derivan de lv_button_class: se marcan para el tic tactil */
    lv_obj_add_flag(lv_msgbox_add_close_button(mb), UI_FLAG_CLICK_SOUND);
    lv_obj_t *btn = lv_msgbox_add_footer_button(mb, btn_text);
    lv_obj_add_flag(btn, UI_FLAG_CLICK_SOUND);
    lv_obj_set_style_bg_color(btn, btn_color, 0);
    lv_obj_set_style_text_color(btn, lv_color_white(), 0);
    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, user_data ? user_data : mb);
    lv_obj_center(mb);
    return mb;
}

/* Feedback sonoro al pulsar cualquier boton (spec sound): enganchado al indev, no a cada
 * widget. El bebe tiene su propio latido y queda fuera. */
static void click_feedback_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_t *obj = lv_indev_get_active_obj();
    if (!obj) return;
    bool is_button = lv_obj_has_class(obj, &lv_button_class) || lv_obj_has_flag(obj, UI_FLAG_CLICK_SOUND);
    bool is_icon = lv_obj_check_type(obj, &lv_image_class) && lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE) &&
                   obj != baby_widget_obj();
    if (is_button || is_icon) sound_request(SOUND_CLICK);
}

/* ----------------------------------------------------------------- splash */

static void build_splash(void)
{
    s_scr_splash = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr_splash, lv_color_white(), 0);
    lv_obj_t *logo = lv_image_create(s_scr_splash);
    lv_image_set_src(logo, &img_logo);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -10);
    lv_obj_t *spin = lv_spinner_create(s_scr_splash);
    lv_spinner_set_anim_params(spin, 1000, 60);
    lv_obj_set_size(spin, 28, 28);
    lv_obj_align(spin, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_arc_color(spin, COL_CORAL, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spin, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spin, 4, LV_PART_MAIN);
}

static void splash_done_cb(lv_timer_t *t)
{
    lv_screen_load_anim(s_scr_home, LV_SCR_LOAD_ANIM_FADE_IN, 400, 0, false);
    lv_timer_delete(t);
}

/* ----------------------------------------------------------------- events */

static void on_state(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_pending = *(const twin_snapshot_t *)data;
    s_state_dirty = true;
    xSemaphoreGive(s_mutex);
}

static void on_settings(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_settings_pending = *(const app_evt_settings_t *)data;
    s_settings_dirty = true;
    xSemaphoreGive(s_mutex);
}

static void refresh_cb(lv_timer_t *t)
{
    (void)t;
    twin_snapshot_t snap;
    app_evt_settings_t st;
    bool do_state, do_settings;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    do_state = s_state_dirty;
    do_settings = s_settings_dirty;
    snap = s_pending;
    st = s_settings_pending;
    s_state_dirty = s_settings_dirty = false;
    xSemaphoreGive(s_mutex);

    if (do_settings) {
        ui_home_retranslate();
        ui_settings_retranslate();
        ui_settings_update(&st);
        do_state = true; /* la barra de estado cambia de idioma */
    }
    if (do_state) {
        ui_home_apply(&snap);
        ui_settings_apply(&snap);
    }
}

/* ------------------------------------------------------------------- init */

void ui_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    s_pending = twin_model_get();
    s_state_dirty = false;

    if (!lvgl_port_lock(2000)) {
        ESP_LOGE(TAG, "sin lock de LVGL");
        return;
    }
    if (board_touch_indev()) {
        lv_indev_set_long_press_time(board_touch_indev(), 1000); /* factory reset: 1 s */
        lv_indev_add_event_cb(board_touch_indev(), click_feedback_cb, LV_EVENT_PRESSED, NULL);
    }
    build_splash();
    s_scr_home = ui_home_create();
    s_scr_settings = ui_settings_create();
    ui_home_apply(&s_pending);
    ui_settings_apply(&s_pending);

    lv_screen_load(s_scr_splash);
    lv_timer_create(splash_done_cb, SPLASH_MS, NULL);
    lv_timer_create(refresh_cb, 50, NULL);
    lvgl_port_unlock();

    app_events_subscribe(TWIN_EVT_STATE_CHANGED, on_state, NULL);
    app_events_subscribe(TWIN_EVT_SETTINGS_CHANGED, on_settings, NULL);
    sound_request(SOUND_BOOT);
    ESP_LOGI(TAG, "ui lista");
}
