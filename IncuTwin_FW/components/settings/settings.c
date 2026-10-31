#include "settings.h"

#include "app_events.h"
#include "board.h"
#include "esp_log.h"
#include "storage.h"

static const char *TAG = "settings";

static volatile uint8_t s_lang = SETTINGS_LANG_ES;
static volatile uint8_t s_vol = SETTINGS_VOL_MAX;
static volatile uint8_t s_bright = SETTINGS_BRIGHT_MAX;

static uint8_t clamp_bright(int v)
{
    if (v < SETTINGS_BRIGHT_MIN) return SETTINGS_BRIGHT_MIN;
    if (v > SETTINGS_BRIGHT_MAX) return SETTINGS_BRIGHT_MAX;
    return (uint8_t)v;
}

static void publish(void)
{
    app_evt_settings_t ev = { .lang = s_lang, .volume = s_vol, .brightness = s_bright };
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
    board_backlight_set(s_bright);
    ESP_LOGI(TAG, "lang=%s vol=%u bright=%u%%", s_lang ? "en" : "es", s_vol, s_bright);
}

uint8_t settings_lang(void) { return s_lang; }

void settings_set_lang(uint8_t lang)
{
    lang = lang ? SETTINGS_LANG_EN : SETTINGS_LANG_ES;
    if (lang == s_lang) return;
    s_lang = lang;
    storage_set_u8(STORAGE_NS_SETTINGS, "lang", lang);
    publish();
}

uint8_t settings_volume(void) { return s_vol; }

void settings_set_volume(uint8_t level)
{
    if (level > SETTINGS_VOL_MAX) level = SETTINGS_VOL_MAX;
    if (level == s_vol) return;
    s_vol = level;
    storage_set_u8(STORAGE_NS_SETTINGS, "vol", level);
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
    publish();
}
