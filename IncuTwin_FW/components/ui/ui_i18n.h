/* Textos ES/EN del panel (tablas de las specs twin-display, settings, onboarding). */
#pragma once

#include "twin_types.h"

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
    STR_STREAK_TITLE,
    STR_WEEKDAY_INITIALS, /* 7 letras, de domingo a sabado */
    STR_PG_TIME_IN,       /* barra de estado en el mapa: pais, hora */
    STR_HH_ST_SUPER_IN,   /* min, s que faltan para el super corazon */
    STR_HH_ST_SUPER,
    STR_STREAK_SUPER,
    STR_HH_BACK,
    STR_HH_ST_AGAIN,
    STR_STREAK_REST,
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
    /* nombre local del bebe (spec nombre-bebe-panel) */
    STR_NAME_ARRIVED,
    STR_NAME_ASK,
    STR_NAME_YES,
    STR_NAME_LATER,
    STR_NAME_PLACEHOLDER,
    STR_NAME_DONE,
    /* avisos de lo que vive el bebe (spec hmi-navegacion-avisos); ver ui_notice_text() */
    STR_NOTICE_ADMISSION,
    STR_NOTICE_PARENTS,
    STR_NOTICE_EXIT,
    STR_NOTICE_HEAT_ON,
    STR_NOTICE_HEAT_OFF,
    STR_NOTICE_PHOTO_ON,
    STR_NOTICE_PHOTO_OFF,
    /* historial */
    STR_HISTORY,
    STR_HISTORY_EMPTY,
    /* ajustes: horario nocturno */
    STR_QUIET,
    STR_QUIET_FROM,
    STR_QUIET_TO,
    STR_COUNT
} ui_str_t;

/* Texto en el idioma actual (settings_lang()). */
const char *tr(ui_str_t id);
/* Nombre del pais en el idioma actual; el propio codigo si no esta en la tabla. */
const char *tr_country(const char *iso);
/* Texto del aviso `kind` (un solo bit de twin_notice_t) en el idioma actual; "" si no es un aviso. */
const char *ui_notice_text(twin_notice_t kind);

#ifdef __cplusplus
}
#endif
