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

static lv_obj_t *s_scr_splash, *s_scr_home, *s_scr_notices, *s_scr_hearts, *s_scr_history, *s_scr_settings;
static ui_tab_t s_tab = UI_TAB_HOME;        /* pantalla de la barra que se ve (o se esta cargando) */
static uint8_t s_lang_shown;                /* idioma de los textos de Notificaciones e Historial */
static uint32_t s_notices_ms;               /* lv_tick del ultimo repintado de Notificaciones */
static uint32_t s_history_ms;               /* lv_tick del ultimo repintado del Historial */
#define HISTORY_TICK_MS 60000               /* «hace N min» al dia mientras se mira */

/* dato pendiente de aplicar (lo escribe el bus de eventos, lo lee la tarea LVGL) */
static SemaphoreHandle_t s_mutex;
static twin_snapshot_t s_pending;
static bool s_state_dirty;
static app_evt_settings_t s_settings_pending;
static bool s_settings_dirty;
static bool s_log_dirty;   /* TWIN_EVT_LOG_CHANGED: las notificaciones han cambiado */
static bool s_stays_dirty; /* TWIN_EVT_STAYS_CHANGED: los bebes de las IncuNest han cambiado */
static uint32_t s_notice_pending; /* bits de TWIN_EVT_NOTICE aun sin pasar a la franja (con demo) */
static uint8_t s_notice_sex;      /* twin_sex_t del ultimo aviso (icono de la franja) */
static bool s_rewards_dirty;      /* TWIN_EVT_REWARDS_CHANGED */
static app_evt_rewards_t s_rewards_news; /* novedades acumuladas (bits OR) para la franja */

/* ------------------------------------------------------------- navigation */

static const char *const TAB_NAME[UI_TAB_COUNT] = { "inicio", "notificaciones", "corazones", "historial", "ajustes" };

static void notices_show(void)
{
    if (event_log_unseen() > 0) event_log_mark_seen(); /* publica LOG_CHANGED: repinta el punto */
    ui_notices_refresh();
    s_notices_ms = lv_tick_get();
    ui_nav_set_unread(false);
}

static void history_show(void)
{
    ui_history_reset(); /* desde la barra se entra siempre por la lista de IncuNest */
    ui_history_refresh();
    s_history_ms = lv_tick_get();
}

void ui_go(ui_tab_t tab)
{
    if (tab == s_tab || tab >= UI_TAB_COUNT) return;
    lv_obj_t *scr[UI_TAB_COUNT] = { s_scr_home, s_scr_notices, s_scr_hearts, s_scr_history, s_scr_settings };
    lv_screen_load_anim_t anim = tab > s_tab ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    ESP_LOGI(TAG, "pantalla: %s -> %s", TAB_NAME[s_tab], TAB_NAME[tab]);
    s_tab = tab;
    ui_nav_set_active(tab);
    if (tab == UI_TAB_NOTICES) notices_show();
    if (tab == UI_TAB_HISTORY) history_show();
    lv_screen_load_anim(scr[tab], anim, 220, 0, false);
}

void ui_nav_update(void)
{
    lv_obj_t *act = lv_screen_active();
    bool ours = act == s_scr_home || act == s_scr_notices || act == s_scr_hearts || act == s_scr_history ||
                act == s_scr_settings;
    ui_nav_set_visible(ours && !(act == s_scr_home && ui_home_overlay_active()));
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
    ui_ota_note_touch(); /* ya no es «recien arrancado» */
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

static void on_stays_changed(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_stays_dirty = true;
    xSemaphoreGive(s_mutex);
}

static void on_rewards(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    const app_evt_rewards_t *r = data;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_rewards_dirty = true;
    s_rewards_news.hearts += r->hearts;
    s_rewards_news.new_seals |= r->new_seals;
    s_rewards_news.level_up |= r->level_up;
    s_rewards_news.garland |= r->garland;
    xSemaphoreGive(s_mutex);
}

static void on_notice(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    const app_evt_notice_t *n = data;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_notice_pending |= n->mask; /* la franja sale tambien en la demo */
    s_notice_sex = n->sex;
    xSemaphoreGive(s_mutex);
}

/* La franja no sale en splash ni onboarding (no es una de nuestras pantallas), ni en la despedida
 * o el agradecimiento de «Coge mi mano», ni con el aviso, un dialogo o el teclado del nombre abiertos. */
static bool notice_blocked(void)
{
    lv_obj_t *act = lv_screen_active();
    bool ours = act == s_scr_home || act == s_scr_notices || act == s_scr_hearts || act == s_scr_history ||
                act == s_scr_settings;
    return !ours || ui_home_overlay_active() || ui_name_open() || ui_notice_layer_busy();
}

static void refresh_cb(lv_timer_t *t)
{
    (void)t;
    twin_snapshot_t snap;
    app_evt_settings_t st;
    bool do_state, do_settings, do_log, do_stays, do_rewards;
    app_evt_rewards_t news;
    uint32_t notices;
    uint8_t notice_sex;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    do_state = s_state_dirty;
    do_settings = s_settings_dirty;
    do_log = s_log_dirty;
    do_stays = s_stays_dirty;
    do_rewards = s_rewards_dirty;
    news = s_rewards_news;
    s_rewards_dirty = false;
    s_rewards_news = (app_evt_rewards_t){ 0 };
    snap = s_pending;
    st = s_settings_pending;
    notices = s_notice_pending;
    notice_sex = s_notice_sex;
    s_notice_pending = 0;
    s_state_dirty = s_settings_dirty = s_log_dirty = s_stays_dirty = false;
    xSemaphoreGive(s_mutex);

    /* event_log y stays toman un mutex (y una escritura a NVS lo retiene unos ms): solo se leen
     * aqui cuando han cambiado, y una vez por minuto mientras se ve su pantalla */
    bool notices_on = s_tab == UI_TAB_NOTICES && lv_screen_active() == s_scr_notices;
    if (do_log) {
        if (notices_on && event_log_unseen() > 0) event_log_mark_seen(); /* lo esta viendo */
        ui_notices_refresh();
        s_notices_ms = lv_tick_get();
        ui_nav_set_unread(event_log_unseen() > 0);
    } else if (notices_on && lv_tick_elaps(s_notices_ms) >= HISTORY_TICK_MS) {
        ui_notices_refresh();
        s_notices_ms = lv_tick_get();
    }
    bool history_on = s_tab == UI_TAB_HISTORY && lv_screen_active() == s_scr_history;
    if (do_stays || (history_on && lv_tick_elaps(s_history_ms) >= HISTORY_TICK_MS)) {
        ui_history_refresh();
        s_history_ms = lv_tick_get();
    }

    if (do_settings) {
        ui_home_retranslate();
        ui_settings_retranslate();
        ui_settings_update(&st);
        if (st.lang != s_lang_shown) { /* no con cada paso del brillo: relee las listas */
            s_lang_shown = st.lang;
            ui_notices_retranslate();
            ui_history_retranslate();
            ui_hearts_refresh();
            ui_shop_refresh();
        }
        do_state = true; /* los textos de Inicio cambian de idioma */
    }
    if (do_state) {
        ui_home_apply(&snap);
        ui_settings_apply(&snap);
        ui_history_apply(&snap);
    }
    if (do_rewards) { /* corazones, sellos o lo elegido */
        ui_rewards_refresh();
        ui_home_rewards_apply();
        ui_hearts_refresh();
        ui_shop_refresh();
        if (news.new_seals || news.level_up || news.garland) {
            ui_notice_push_reward(news.new_seals, news.level_up, (uint8_t)rw_level(ui_rewards()->earned),
                                  news.garland);
        }
    }
    ui_ota_poll(); /* ventana de la actualizacion: cuando toca, y su barra de progreso */
    ui_incupray_poll(); /* IncuPray: avisos, boton de Inicio y su pantalla (nada sin horario) */
    ui_nav_update(); /* tras el splash, el cambio de WiFi o el final del gesto */
    if (notices) ui_notice_push(notices, notice_sex);
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
    ui_rewards_refresh(); /* el tema elegido, antes de pintar Inicio */
    s_scr_home = ui_home_create();
    s_scr_notices = ui_notices_create();
    s_scr_history = ui_history_create();
    s_scr_hearts = ui_hearts_create();
    ui_shop_create(); /* encima de Corazones, oculta */
    s_scr_settings = ui_settings_create();
    ui_home_rewards_apply();
    ui_nav_create(); /* antes de la franja: los dialogos que se abran luego quedan encima */
    ui_nav_set_unread(event_log_unseen() > 0);
    ui_notice_init();
    ui_home_apply(&s_pending);
    ui_settings_apply(&s_pending);
    ui_history_apply(&s_pending);

    lv_screen_load(s_scr_splash);
    lv_timer_create(splash_done_cb, SPLASH_MS, NULL);
    lv_timer_create(refresh_cb, 50, NULL);
    lvgl_port_unlock();

    app_events_subscribe(TWIN_EVT_STATE_CHANGED, on_state, NULL);
    app_events_subscribe(TWIN_EVT_SETTINGS_CHANGED, on_settings, NULL);
    app_events_subscribe(TWIN_EVT_LOG_CHANGED, on_log_changed, NULL);
    app_events_subscribe(TWIN_EVT_STAYS_CHANGED, on_stays_changed, NULL);
    app_events_subscribe(TWIN_EVT_REWARDS_CHANGED, on_rewards, NULL);
    app_events_subscribe(TWIN_EVT_NOTICE, on_notice, NULL);
    /* el jingle solo al enchufar: un reinicio sin nadie delante (OTA, comando, watchdog) no suena */
    esp_reset_reason_t rr = esp_reset_reason();
    if (rr == ESP_RST_POWERON) {
        sound_request(SOUND_BOOT);
    }
    ESP_LOGI(TAG, "motivo de reinicio %d: %s", (int)rr, rr == ESP_RST_POWERON ? "suena el arranque" : "sin melodia de arranque");
    ESP_LOGI(TAG, "ui lista");
}
