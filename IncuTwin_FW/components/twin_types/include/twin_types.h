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
/* Pulso que se ensena (latido, corazon, numero): solo el normal de un recien nacido. Fuera de
 * este rango casi siempre es una mala lectura del sensor y solo asustaria (Pablo, 21-nov-2026). */
#define TWIN_BPM_SHOW_MIN 100
#define TWIN_BPM_SHOW_MAX 180
/* A partir de aqui un adulto lo veria rapido (su normal en reposo es <= 100): se explica que el
 * corazon de los bebes late muy rapido. */
#define TWIN_BPM_FAST 110
#define TWIN_SKIN_COUNT 5
#define TWIN_SKIN_DARK_MIN 3 /* tonos 3..4: familia de piel oscura en "con sus papas" */
#define TWIN_COUNTRY_LEN 3      /* ISO 3166-1 alfa-2 en mayusculas + 0 */

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
    uint8_t skin;         /* 0..4; "skin" del estado o, si nunca vino, el del pais */
    bool skin_set;        /* llego "skin" explicito: ya no se deduce del pais */
    char name[TWIN_NAME_LEN]; /* "" = no compartido */
    char country[TWIN_COUNTRY_LEN]; /* pais de la incubadora ("CM"); "" = no compartido */
    uint16_t weight_g;    /* 0 = no compartido */
    int16_t age_d;        /* -1 = no compartido */
} twin_incubator_t;

/* Fase del modo Acompañar (spec accompany). */
typedef enum {
    TWIN_ACC_OFF = 0,   /* modo apagado: emparejado fijo o sin emparejar */
    TWIN_ACC_SEARCHING, /* sin IncuNest: buscando una con bebe */
    TWIN_ACC_WAITING,   /* bebe elegido, falta el primer gesto: "Un bebe te espera" */
    TWIN_ACC_WITH,      /* acompañando */
    TWIN_ACC_ENDING,    /* su IncuNest sin bebe o desconectada: margen antes de buscar otra */
    TWIN_ACC_FAREWELL,  /* se fue a casa: despedida hasta tocar la pantalla o el dia siguiente */
} twin_acc_t;

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
    /* modo Acompañar (TWIN_ACC_OFF si esta apagado o en la demo) */
    twin_acc_t acc;
    bool acc_remind;       /* "¿Le das la mano hoy?" */
    uint8_t acc_week;      /* corazones: bit 0 = hoy ... bit 6 = hace 6 dias */
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
    /* modo Acompañar */
    TWIN_ST_ACC_SEARCH,   /* "Ahora no hay bebes. Te avisare" */
    TWIN_ST_ACC_WAITING,  /* "Un bebe te espera" */
    TWIN_ST_ACC_REMIND,   /* "¿Le das la mano hoy?" */
    TWIN_ST_ACC_ENDED,    /* "Ha terminado su estancia" */
    TWIN_ST_ACC_FAREWELL, /* "¡Se ha ido a casa! Gracias por acompañarle" */
} twin_status_t;

#ifdef __cplusplus
}
#endif
