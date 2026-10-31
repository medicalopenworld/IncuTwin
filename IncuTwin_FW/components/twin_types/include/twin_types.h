/* IncuTwin — tipos del modelo del gemelo (vocabulario de las specs twin-display y
 * twin-state-ingest). Solo datos: sin dependencias de IDF ni LVGL. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TWIN_INCUBATOR_ID_LEN 17 /* 1..16 chars [A-Za-z0-9_-] + 0 */
#define TWIN_NAME_LEN 24         /* nombre del bebe: <= 23 bytes UTF-8 + 0 */
#define TWIN_BPM_MAX 300
#define TWIN_SKIN_COUNT 6

typedef enum {
    TWIN_BABY_NONE = 0, /* incubadora libre */
    TWIN_BABY_IN,       /* bebe dentro */
    TWIN_BABY_PARENTS,  /* con sus padres (canguro) — extension */
    TWIN_BABY_HOME,     /* alta a casa — extension */
} twin_baby_t;

typedef enum {
    TWIN_THERMO_OFF = 0,
    TWIN_THERMO_HEATING, /* extension */
    TWIN_THERMO_STABLE,  /* "heat" en treatments */
    TWIN_THERMO_ALARM,   /* extension */
} twin_thermo_t;

/* Lo que dice la incubadora (ultimo estado aplicado). */
typedef struct {
    bool online;          /* state != "offline" */
    twin_baby_t baby;
    twin_thermo_t thermo;
    bool photo;           /* "phototherapy" en treatments */
    bool pulseox;         /* "pulseox" en treatments */
    uint16_t bpm;         /* 0 = sin pulso */
    bool awake;           /* extension; false por defecto */
    uint8_t skin;         /* 0..5; extension, por defecto Kconfig */
    char name[TWIN_NAME_LEN]; /* "" = no compartido */
    uint16_t weight_g;    /* 0 = no compartido */
    int16_t age_d;        /* -1 = no compartido */
} twin_incubator_t;

/* Estado del enlace del panel. */
typedef struct {
    bool wifi;
    int8_t rssi;
    char ip[16];
    bool has_creds;        /* credenciales MQTT de fabrica presentes */
    bool broker_connected;
    bool broker_once;      /* ha conectado al menos una vez desde el arranque */
    bool broker_lost;      /* >= INCUTWIN_BROKER_LOST_S sin sesion tras haberla tenido */
    bool paired;           /* hay incubator_id */
    bool state_rx;         /* estado recibido desde la ultima suscripcion */
    char incubator_id[TWIN_INCUBATOR_ID_LEN];
} twin_link_t;

/* Copia inmutable que publica el modelo (TWIN_EVT_STATE_CHANGED). */
typedef struct {
    twin_link_t link;
    twin_incubator_t inc;
    bool demo;             /* overlay del modo demo activo */
    /* derivados (twin_derive) */
    bool link_ok;
    bool show_baby;
    bool parents_view;
} twin_snapshot_t;

/* Transiciones que suenan (TWIN_EVT_TRANSITION). */
typedef enum {
    TWIN_TR_BABY_IN = 1,      /* -> in desde otro valor */
    TWIN_TR_BABY_PARENTS = 2, /* -> parents/home desde otro valor */
} twin_transition_t;

/* Linea de la barra de estado, por prioridad (spec twin-display). */
typedef enum {
    TWIN_ST_HOME = 0,
    TWIN_ST_PARENTS,
    TWIN_ST_NO_WIFI,
    TWIN_ST_NO_CREDS,
    TWIN_ST_CONNECTING,
    TWIN_ST_BROKER_LOST,
    TWIN_ST_UNPAIRED,
    TWIN_ST_WAITING,
    TWIN_ST_INC_OFF,
    TWIN_ST_NO_BABY,
    TWIN_ST_ALARM,
    TWIN_ST_NAME,
    TWIN_ST_AWAKE,
    TWIN_ST_SLEEP,
} twin_status_t;

#ifdef __cplusplus
}
#endif
