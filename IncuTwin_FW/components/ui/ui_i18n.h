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
    STR_INFO_PHOTO,
    STR_INFO_HEART,       /* sin el numero: el latido se oye y se ve al ritmo real */
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
    /* mapa: hora del pais debajo de su nombre (%s = "HH:MM") */
    STR_PG_TIME_THERE,
    /* historial por incubadora > bebe > avisos */
    STR_HIST_BABY,
    STR_HIST_NOW,
    STR_HIST_ARRIVED, /* %s = «hace 3 d» */
    STR_HIST_LEFT,    /* %s = «hace 3 d» */
    STR_HIST_MORE,    /* %u = bebes de antes que ya no estan guardados */
    /* notificaciones (todos los cambios de estado) */
    STR_NOTICES,
    STR_NOTICES_EMPTY,
    /* recompensas (spec rewards): niveles, sellos y sus burbujas, pasaporte, franjas, personalizar */
    STR_LEVEL_SPARK,
    STR_LEVEL_WARMTH,
    STR_LEVEL_HUG,
    STR_LEVEL_BLANKET,
    STR_LEVEL_SUN,
    STR_SEAL_FIRST,
    STR_SEAL_WEEK,
    STR_SEAL_MONTH_RUN,
    STR_SEAL_30_DAYS,
    STR_SEAL_SUPER,
    STR_SEAL_3_BABIES,
    STR_SEAL_HOME,
    STR_SEAL_FIRST_D,
    STR_SEAL_WEEK_D,
    STR_SEAL_MONTH_RUN_D,
    STR_SEAL_30_DAYS_D,
    STR_SEAL_SUPER_D,
    STR_SEAL_3_BABIES_D,
    STR_SEAL_HOME_D,
    STR_PASSPORT,
    STR_PASSPORT_EMPTY,
    STR_RW_NEW_SEAL,
    STR_RW_LEVEL_UP,
    STR_RW_GARLAND,
    STR_CUSTOMIZE,
    STR_CUST_HALO,
    STR_CUST_BG,
    STR_CUST_SEASON,
    STR_CUST_CHOSEN,
    STR_CUST_USE,
    STR_CUST_UNLOCK_Q,
    STR_CUST_UNLOCK,
    STR_CUST_MISSING,
    STR_CUST_GARLAND_HINT,
    STR_OK,
    STR_ITEM_HALO_SKY,
    STR_ITEM_HALO_LAVENDER,
    STR_ITEM_HALO_MINT,
    STR_ITEM_HALO_PEACH,
    STR_ITEM_HALO_LILAC,
    STR_ITEM_BG_DEFAULT,
    STR_ITEM_BG_CREAM,
    STR_ITEM_BG_PINK,
    STR_ITEM_BG_AQUA,
    STR_ITEM_GARLAND,
    STR_HEARTS,
    STR_HEARTS_NEXT,
    STR_HEARTS_MAX,
    STR_HEARTS_STREAK,
    STR_SEALS,
    STR_SHOP,
    STR_CUST_SPECIAL,
    STR_ITEM_NAMED_INCUNEST,
    STR_REDEEM_Q,
    STR_REDEEM,
    STR_REDEEMED,
    STR_REDEEM_DEMO,
    STR_OTA_UPDATING, /* ventana de la actualizacion (spec ota-update) */
    STR_OTA_DONT_UNPLUG,
    STR_OTA_RESTARTING,
    STR_OTA_FAILED,
    STR_TWIN_NAME, /* nombre de la IncuTwin (Ajustes) */
    STR_TWIN_NAME_HINT,
    STR_IP_EVE, /* IncuPray (spec incupray): nunca habla de rezar */
    STR_IP_HOUR,
    STR_IP_START,
    STR_IP_SUMMARY,
    STR_IP_LEFT,
    STR_IP_COUNT,
    STR_IP_PICK,
    STR_IP_NO_BABIES,
    STR_IP_HOLD,
    STR_IP_HEARTS,
    STR_IP_STAKE,
    STR_IP_FREE,
    STR_IP_MAP,
    STR_IP_EXIT,
    STR_IP_THANKS,
    STR_IP_THANKS_D,
    STR_IP_THANKS_ALL,
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
