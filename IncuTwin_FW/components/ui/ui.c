#include "ui.h"

#include <string.h>

#include "assets.h"
#include "baby_widget.h"
#include "board.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_lvgl_port.h"
#include "event_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "settings.h"
#include "sound.h"
#include "twin_model.h"
#include "ui_internal.h"
#include "ui_name.h"

static const char *TAG = "ui";

#define SPLASH_MS 2500

static lv_obj_t *s_scr_splash, *s_scr_home, *s_scr_history, *s_scr_settings;
static ui_tab_t s_tab = UI_TAB_HOME;        /* pantalla de la barra que se ve (o se esta cargando) */
static uint8_t s_lang_shown;                /* idioma de los textos del Historial */
static uint32_t s_history_ms;               /* lv_tick del ultimo repintado del Historial */
#define HISTORY_TICK_MS 60000               /* «hace N min» al dia mientras se mira */

/* dato pendiente de aplicar (lo escribe el bus de eventos, lo lee la tarea LVGL) */
static SemaphoreHandle_t s_mutex;
static twin_snapshot_t s_pending;
static bool s_state_dirty;
static app_evt_settings_t s_settings_pending;
static bool s_settings_dirty;
static bool s_log_dirty; /* TWIN_EVT_LOG_CHANGED: el historial ha cambiado */
static uint32_t s_notice_pending; /* bits de TWIN_EVT_NOTICE aun sin pasar a la franja (con demo) */

/* ------------------------------------------------------------- navigation */

static const char *const TAB_NAME[] = { "inicio", "historial", "ajustes" };

static void history_show(void)
{
    if (event_log_unseen() > 0) event_log_mark_seen(); /* publica LOG_CHANGED: repinta el punto */
    ui_history_refresh();
    s_history_ms = lv_tick_get();
    ui_nav_set_unread(false);
}

void ui_go(ui_tab_t tab)
{
    if (tab == s_tab || tab > UI_TAB_SETTINGS) return;
    lv_obj_t *scr[] = { s_scr_home, s_scr_history, s_scr_settings };
    lv_screen_load_anim_t anim = tab > s_tab ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    ESP_LOGI(TAG, "pantalla: %s -> %s", TAB_NAME[s_tab], TAB_NAME[tab]);
    s_tab = tab;
    if (tab == UI_TAB_HISTORY) history_show();
    lv_screen_load_anim(scr[tab], anim, 220, 0, false);
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

/* Feedback sonoro solo al pulsar un boton (spec sound): enganchado al indev, no a cada widget.
 * Cuenta como boton un lv_button, un interruptor (horario nocturno), lo marcado con
 * UI_FLAG_CLICK_SOUND (botones del msgbox, simbolos de tratamiento) o una imagen pulsable.
 * Tocar el bebe o una zona vacia no suena: el tactil da a veces pulsaciones fantasma y no deben
 * oirse (Pablo, 21-nov-2026). */
static void click_feedback_cb(lv_event_t *e)
{
    (void)e;
    if (lv_screen_active() == s_scr_home) ui_home_screen_pressed();
    lv_obj_t *obj = lv_indev_get_active_obj();
    if (!obj) return;
    bool is_button = lv_obj_has_class(obj, &lv_button_class) || lv_obj_has_class(obj, &lv_switch_class) ||
                     lv_obj_has_flag(obj, UI_FLAG_CLICK_SOUND);
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

static void on_log_changed(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_log_dirty = true;
    xSemaphoreGive(s_mutex);
}

static void on_notice(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_notice_pending |= ((const app_evt_notice_t *)data)->mask; /* la franja sale tambien en la demo */
    xSemaphoreGive(s_mutex);
}

/* La franja no sale en splash ni onboarding (no es una de nuestras pantallas), ni en la despedida
 * o el agradecimiento de «Coge mi mano», ni con el aviso o el teclado del nombre abiertos. */
static bool notice_blocked(void)
{
    lv_obj_t *act = lv_screen_active();
    bool ours = act == s_scr_home || act == s_scr_history || act == s_scr_settings;
    return !ours || ui_home_overlay_active() || ui_name_open();
}

static void refresh_cb(lv_timer_t *t)
{
    (void)t;
    twin_snapshot_t snap;
    app_evt_settings_t st;
    bool do_state, do_settings, do_log;
    uint32_t notices;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    do_state = s_state_dirty;
    do_settings = s_settings_dirty;
    do_log = s_log_dirty;
    snap = s_pending;
    st = s_settings_pending;
    notices = s_notice_pending;
    s_notice_pending = 0;
    s_state_dirty = s_settings_dirty = s_log_dirty = false;
    xSemaphoreGive(s_mutex);

    /* event_log toma un mutex (y una escritura a NVS lo retiene unos ms): solo se lee aqui
     * cuando el historial ha cambiado, y una vez por minuto mientras se ve el Historial */
    bool history_on = s_tab == UI_TAB_HISTORY && lv_screen_active() == s_scr_history;
    if (do_log) {
        if (history_on && event_log_unseen() > 0) event_log_mark_seen(); /* lo esta viendo */
        ui_history_refresh();
        s_history_ms = lv_tick_get();
        ui_nav_set_unread(event_log_unseen() > 0);
    } else if (history_on && lv_tick_elaps(s_history_ms) >= HISTORY_TICK_MS) {
        ui_history_refresh();
        s_history_ms = lv_tick_get();
    }

    if (do_settings) {
        ui_home_retranslate();
        ui_settings_retranslate();
        ui_settings_update(&st);
        if (st.lang != s_lang_shown) { /* no con cada paso del brillo: relee el historial */
            s_lang_shown = st.lang;
            ui_history_retranslate();
        }
        do_state = true; /* la barra de estado cambia de idioma */
    }
    if (do_state) {
        ui_home_apply(&snap);
        ui_settings_apply(&snap);
    }
    if (notices) ui_notice_push(notices);
    ui_notice_poll(notice_blocked()); /* tras ui_home_apply: overlay_active ya esta al dia */
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
    s_lang_shown = settings_lang();
    s_scr_home = ui_home_create();
    s_scr_history = ui_history_create();
    s_scr_settings = ui_settings_create();
    ui_nav_set_unread(event_log_unseen() > 0);
    ui_notice_init();
    ui_home_apply(&s_pending);
    ui_settings_apply(&s_pending);

    lv_screen_load(s_scr_splash);
    lv_timer_create(splash_done_cb, SPLASH_MS, NULL);
    lv_timer_create(refresh_cb, 50, NULL);
    lvgl_port_unlock();

    app_events_subscribe(TWIN_EVT_STATE_CHANGED, on_state, NULL);
    app_events_subscribe(TWIN_EVT_SETTINGS_CHANGED, on_settings, NULL);
    app_events_subscribe(TWIN_EVT_LOG_CHANGED, on_log_changed, NULL);
    app_events_subscribe(TWIN_EVT_NOTICE, on_notice, NULL);
    /* el jingle solo al enchufar: un reinicio sin nadie delante (OTA, comando, watchdog) no suena */
    esp_reset_reason_t rr = esp_reset_reason();
    if (rr == ESP_RST_POWERON) {
        sound_request(SOUND_BOOT);
    }
    ESP_LOGI(TAG, "motivo de reinicio %d: %s", (int)rr, rr == ESP_RST_POWERON ? "suena el arranque" : "sin melodia de arranque");
    ESP_LOGI(TAG, "ui lista");
}
