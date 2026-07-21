#pragma once

/* Tiny ES/EN string table. Keep every string short: the UI is meant to be
 * readable by children and elderly users. */

typedef enum {
    STR_CONNECTION = 0,
    STR_WARMTH,
    STR_LIGHT,
    STR_HEART,
    STR_ON,
    STR_OFF,
    STR_HEATING,
    STR_STABLE,
    STR_ALARM,
    STR_NO_DATA,
    STR_CONNECTED,
    STR_NO_CONNECTION,
    STR_SETTINGS,
    STR_LANGUAGE,
    STR_SKIN_TONE,
    STR_SKIN_HINT,
    STR_BACK,
    STR_WIFI,
    STR_CLOUD,
    STR_VERSION,
    STR_SLEEPING,
    STR_AWAKE,
    STR_BEATING,
    STR_SOUND,
    /* status bar + hand button */
    STR_ST_NO_WIFI,
    STR_ST_CONNECTING,
    STR_ST_UNLINKED,
    STR_ST_OFF,
    STR_ST_NO_BABY,
    STR_ST_BABY_SLEEP,
    STR_ST_BABY_AWAKE,
    STR_HAND,
    /* onboarding */
    STR_OB_LANG_TITLE,
    STR_OB_CONNECT_TITLE,
    STR_OB_CONNECT_STEPS,
    STR_OB_NETWORK,
    STR_OB_CONNECTING,
    STR_OB_REGISTERING,
    STR_OB_WIFI_FAIL,
    STR_OB_PAIR_TITLE,
    STR_OB_PAIR_STEPS,
    STR_OB_FINISH,
    STR_COUNT
} str_id_t;

#ifdef __cplusplus
extern "C" {
#endif

extern int g_lang; /* 0 = ES, 1 = EN */

const char *tr(str_id_t id);
void i18n_set_lang(int lang); /* persists to NVS */
void i18n_load(void);

#ifdef __cplusplus
}
#endif
