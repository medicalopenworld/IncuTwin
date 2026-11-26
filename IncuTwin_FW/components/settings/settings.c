#include "settings.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_events.h"
#include "board.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "storage.h"
#include "telemetry.h"

static const char *TAG = "settings";

static volatile uint8_t s_lang = SETTINGS_LANG_ES;
static volatile uint8_t s_vol = SETTINGS_VOL_MAX;
static volatile uint8_t s_bright = SETTINGS_BRIGHT_MAX;

static volatile uint8_t s_q_on = SETTINGS_QUIET_ON_DEF;
static volatile uint8_t s_q_from = SETTINGS_QUIET_FROM_DEF;
static volatile uint8_t s_q_to = SETTINGS_QUIET_TO_DEF;

/* Telemetria `setting`: key y valor numerico (nunca texto) */
static void tel_setting(const char *key, unsigned val)
{
    char d[48];
    snprintf(d, sizeof d, "{\"key\":\"%s\",\"val\":%u}", key, val);
    telemetry_log("setting", d);
}

/* El deslizador de brillo cambia en cada paso del arrastre: solo se registra el valor final, 2 s
 * despues del ultimo cambio, y solo si difiere del ultimo registrado. */
#define BRIGHT_TEL_MS 2000
static esp_timer_handle_t s_bright_timer;
static uint8_t s_bright_logged = 0xFF;

static void bright_timer_cb(void *arg)
{
    (void)arg;
    uint8_t b = s_bright;
    if (b == s_bright_logged) return;
    s_bright_logged = b;
    tel_setting("bright", b);
}

static void bright_tel_schedule(void)
{
    if (!s_bright_timer) {
        const esp_timer_create_args_t a = { .callback = bright_timer_cb, .name = "tel_bright" };
        if (esp_timer_create(&a, &s_bright_timer) != ESP_OK) {
            s_bright_timer = NULL;
            return;
        }
    }
    esp_timer_stop(s_bright_timer); /* ESP_ERR_INVALID_STATE si no corria: da igual */
    esp_timer_start_once(s_bright_timer, BRIGHT_TEL_MS * 1000ULL);
}

static uint8_t clamp_bright(int v)
{
    if (v < SETTINGS_BRIGHT_MIN) return SETTINGS_BRIGHT_MIN;
    if (v > SETTINGS_BRIGHT_MAX) return SETTINGS_BRIGHT_MAX;
    return (uint8_t)v;
}

static void publish(void)
{
    app_evt_settings_t ev = { .lang = s_lang, .volume = s_vol, .brightness = s_bright,
                              .quiet_on = s_q_on, .quiet_from = s_q_from, .quiet_to = s_q_to };
    app_events_post(TWIN_EVT_SETTINGS_CHANGED, &ev, sizeof(ev));
}

void settings_init(void)
{
    uint8_t v;
    if (storage_get_u8(STORAGE_NS_SETTINGS, "lang", &v) && v <= SETTINGS_LANG_EN) {
        s_lang = v;
    } else {
        storage_set_u8(STORAGE_NS_SETTINGS, "lang", s_lang);
    }
    if (storage_get_u8(STORAGE_NS_SETTINGS, "vol", &v) && v <= SETTINGS_VOL_MAX) {
        s_vol = v;
    } else {
        storage_set_u8(STORAGE_NS_SETTINGS, "vol", s_vol);
    }
    if (storage_get_u8(STORAGE_NS_SETTINGS, "bright", &v) && v >= SETTINGS_BRIGHT_MIN &&
        v <= SETTINGS_BRIGHT_MAX) {
        s_bright = v;
    } else {
        storage_set_u8(STORAGE_NS_SETTINGS, "bright", s_bright);
    }
    if (storage_get_u8(STORAGE_NS_SETTINGS, "q_on", &v) && v <= 1) {
        s_q_on = v;
    } else {
        storage_set_u8(STORAGE_NS_SETTINGS, "q_on", s_q_on);
    }
    if (storage_get_u8(STORAGE_NS_SETTINGS, "q_from", &v) && v <= 23) {
        s_q_from = v;
    } else {
        storage_set_u8(STORAGE_NS_SETTINGS, "q_from", s_q_from);
    }
    if (storage_get_u8(STORAGE_NS_SETTINGS, "q_to", &v) && v <= 23) {
        s_q_to = v;
    } else {
        storage_set_u8(STORAGE_NS_SETTINGS, "q_to", s_q_to);
    }
    board_backlight_set(s_bright);
    s_bright_logged = s_bright;
    ESP_LOGI(TAG, "lang=%s vol=%u bright=%u%% quiet=%u %u-%u", s_lang ? "en" : "es", s_vol, s_bright,
             s_q_on, s_q_from, s_q_to);
}

uint8_t settings_lang(void) { return s_lang; }

void settings_set_lang(uint8_t lang)
{
    lang = lang ? SETTINGS_LANG_EN : SETTINGS_LANG_ES;
    if (lang == s_lang) return;
    s_lang = lang;
    storage_set_u8(STORAGE_NS_SETTINGS, "lang", lang);
    tel_setting("lang", lang);
    publish();
}

uint8_t settings_volume(void) { return s_vol; }

void settings_set_volume(uint8_t level)
{
    if (level > SETTINGS_VOL_MAX) level = SETTINGS_VOL_MAX;
    if (level == s_vol) return;
    s_vol = level;
    storage_set_u8(STORAGE_NS_SETTINGS, "vol", level);
    tel_setting("vol", level);
    publish();
}

uint8_t settings_brightness(void) { return s_bright; }

void settings_set_brightness(uint8_t percent)
{
    uint8_t b = clamp_bright(percent);
    board_backlight_set(b);
    if (b == s_bright) return;
    s_bright = b;
    storage_set_u8(STORAGE_NS_SETTINGS, "bright", b);
    bright_tel_schedule();
    publish();
}

uint8_t settings_quiet_on(void) { return s_q_on; }

void settings_set_quiet_on(uint8_t on)
{
    on = on ? 1 : 0;
    if (on == s_q_on) return;
    s_q_on = on;
    storage_set_u8(STORAGE_NS_SETTINGS, "q_on", on);
    tel_setting("quiet_on", on);
    publish();
}

uint8_t settings_quiet_from(void) { return s_q_from; }

void settings_set_quiet_from(uint8_t hour)
{
    if (hour > 23) hour = SETTINGS_QUIET_FROM_DEF;
    if (hour == s_q_from) return;
    s_q_from = hour;
    storage_set_u8(STORAGE_NS_SETTINGS, "q_from", hour);
    tel_setting("quiet_from", hour);
    publish();
}

uint8_t settings_quiet_to(void) { return s_q_to; }

void settings_set_quiet_to(uint8_t hour)
{
    if (hour > 23) hour = SETTINGS_QUIET_TO_DEF;
    if (hour == s_q_to) return;
    s_q_to = hour;
    storage_set_u8(STORAGE_NS_SETTINGS, "q_to", hour);
    tel_setting("quiet_to", hour);
    publish();
}

int settings_local_hour(void)
{
    time_t now = time(NULL);
    if (now < 1700000000) return -1;
    struct tm tm;
    localtime_r(&now, &tm);
    return tm.tm_hour;
}

/* ------------------------------------------------------------------ nombre */

void settings_name(char *out, size_t n)
{
    if (!out || !n) return;
    out[0] = 0;
    if (!storage_get_str(STORAGE_NS_SETTINGS, "twin_name", out, n)) out[0] = 0;
}

void settings_set_name(const char *name)
{
    char cur[SETTINGS_NAME_BYTES];
    settings_name(cur, sizeof(cur));
    char val[SETTINGS_NAME_BYTES];
    snprintf(val, sizeof(val), "%s", name ? name : "");
    if (!strcmp(cur, val)) return;
    if (val[0]) storage_set_str(STORAGE_NS_SETTINGS, "twin_name", val);
    else storage_erase_key(STORAGE_NS_SETTINGS, "twin_name");
    ESP_LOGI(TAG, "nombre de la IncuTwin %s", val[0] ? "puesto" : "borrado"); /* el nombre no va al log */
    app_events_post(TWIN_EVT_NAME_CHANGED, NULL, 0);
}
