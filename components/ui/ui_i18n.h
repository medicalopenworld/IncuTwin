/* Textos ES/EN del panel (tablas de las specs twin-display, settings, onboarding). */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /* barra de estado */
    STR_ST_HOME = 0,
    STR_ST_PARENTS,
    STR_ST_NO_WIFI,
    STR_ST_NO_CREDS,
    STR_ST_CONNECTING,
    STR_ST_BROKER_LOST,
    STR_ST_UNPAIRED,
    STR_ST_WAITING,
    STR_ST_INC_OFF,
    STR_ST_NO_BABY,
    STR_ST_ALARM,
    STR_ST_AWAKE,
    STR_ST_SLEEP,
    STR_DAY,
    STR_DAYS,
    /* pantalla principal */
    STR_HAND,
    /* ajustes */
    STR_SETTINGS,
    STR_LANGUAGE,
    STR_SKIN_TONE,
    STR_SKIN_HINT,
    STR_SOUND,
    STR_VOL_OFF,
    STR_VOL_LOW,
    STR_VOL_MID,
    STR_VOL_HIGH,
    STR_BRIGHTNESS,
    STR_BACK,
    STR_WIFI,
    STR_SERVER,
    STR_CONNECTED,
    STR_NO_CONNECTION,
    STR_INCUNEST,
    STR_RESET_Q,
    STR_RESET,
    /* onboarding */
    STR_OB_CONNECT_TITLE,
    STR_OB_CONNECT_STEPS,
    STR_OB_NETWORK,
    STR_OB_CONNECTING_WIFI,
    STR_OB_CONNECTING_SERVER,
    STR_OB_WIFI_FAIL,
    STR_OB_DONE_TITLE,
    STR_OB_DONE_TEXT,
    STR_OB_FINISH,
    STR_OB_CANCEL,
    STR_WIFI_CHANGE_Q,
    STR_WIFI_CHANGE,
    STR_COUNT
} ui_str_t;

/* Texto en el idioma actual (settings_lang()). */
const char *tr(ui_str_t id);

#ifdef __cplusplus
}
#endif
