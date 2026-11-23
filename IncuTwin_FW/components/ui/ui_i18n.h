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
    /* modo Acompañar (spec accompany) */
    STR_ACC_SEARCH,
    STR_ACC_WAITING,
    STR_ACC_REMIND,
    STR_ACC_ENDED,
    STR_ACC_FAREWELL,
    /* al pulsar los corazones de la racha (acc_streak_msg); %d = dias */
    STR_STREAK_NONE,
    STR_STREAK_START,
    STR_STREAK_TODAY1,
    STR_STREAK_RUN,
    STR_STREAK_WEEK,
    STR_STREAK_KEEP1,
    STR_STREAK_KEEP,
    STR_STREAK_NO_TIME,
    STR_DAY,
    STR_DAYS,
    /* pantalla principal */
    STR_HAND,
    /* info al tocar los simbolos de tratamiento y al bebe en reposo */
    STR_INFO_HEATING,
    STR_INFO_STABLE,
    STR_INFO_TEMP_ALARM,
    STR_INFO_PHOTO,
    STR_INFO_HEART,       /* formato: %u latidos por minuto */
    STR_INFO_HEART_FAST,  /* solo si el pulso pasa de TWIN_BPM_FAST */
    STR_INFO_PRESS_HAND,
    /* gesto "Coge mi mano" (spec twin-display) */
    STR_HH_READY,
    STR_HH_HOLD1,
    STR_HH_HOLD2,
    STR_HH_GRACE,
    STR_HH_THANKS,
    STR_HH_ST_READY,
    STR_HH_ST_HOLD,
    STR_HH_ST_GRACE, /* formato: %u segundos */
    STR_HH_ST_DONE,
    /* paginas de la demo (spec twin-display) */
    STR_PG_HOSPITAL,
    STR_PG_LOCAL_TIME,
    STR_PG_DISTANCE, /* formato: %s km */
    STR_PG_ACHIEVEMENTS,
    STR_PG_ACH_SUB,
    STR_PG_MY_DAYS,
    STR_PG_STREAK,
    STR_PG_FAMILIES,
    STR_PG_DAYS,
    /* ajustes */
    STR_SETTINGS,
    STR_LANGUAGE,
    STR_SKIN_TONE,
    STR_SKIN_HINT,
    STR_SKIN_HINT_DEMO,
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
/* Nombre del pais en el idioma actual; el propio codigo si no esta en la tabla. */
const char *tr_country(const char *iso);

#ifdef __cplusplus
}
#endif
