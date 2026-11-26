/* IncuTwin — logica pura del modo Acompañar (spec accompany, openspec hmi-accompany).
 *
 * Sin IDF, sin LVGL, sin estado global: el tiempo (s monotonos), el dia local (dias desde
 * 1970 en hora de España, o ACC_NO_DAY sin hora valida) y la hora local entran como parametros.
 * accompany.c es el pegamento (NVS, PSRAM, eventos). Probado en test_apps. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "twin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ACC_MAX_INC     64          /* IncuNest que se recuerdan a la vez (tabla en RAM) */
#define ACC_HIST_MAX    32          /* IncuNest en el historial (NVS) */
#define ACC_ENDING_S    (2 * 3600)  /* sin bebe o desconectada antes de buscar otra */
#define ACC_SAVE_S      600         /* guardar el historial cada 10 min mientras cuenta */
#define ACC_REMIND_HOUR 19          /* recordatorio diario */
#define ACC_NO_DAY      INT32_MIN   /* dia desconocido / nunca */
#define ACC_PERSIST_VERSION 1

/* Resultado de cada entrada (mascara) */
#define ACC_R_SWITCH  0x01u /* cambio la IncuNest actual (puede ser a ninguna) */
#define ACC_R_PHASE   0x02u /* cambio la fase o lo que se presenta */
#define ACC_R_SAVE    0x04u /* hay que guardar acc_persist_t */
#define ACC_R_REMIND  0x08u /* toca la llamada del recordatorio (una vez al dia) */
#define ACC_R_FORWARD 0x10u /* el estado recibido es de la actual: aplicarlo al gemelo */

typedef struct {
    char id[TWIN_INCUBATOR_ID_LEN];
    uint32_t secs;
} acc_hist_t;

/* Lo que se guarda en NVS (blob acc/state). */
typedef struct {
    uint8_t version;
    uint8_t phase;          /* twin_acc_t (nunca TWIN_ACC_OFF) */
    uint8_t engaged;        /* ya hubo gesto con el bebe actual */
    uint8_t days_mask;      /* bit i = dia mask_day - i con gesto */
    char cur[TWIN_INCUBATOR_ID_LEN]; /* "" = ninguna */
    /* bit i = dia mask_day - i con super corazon (3 min de mano). Ocupa el relleno que habia
     * tras cur (a 0 en los blobs guardados antes): mismo tamaño, sin cambiar de version. */
    uint8_t super_mask;
    /* Dias sin bebes, para que no rompan la racha (mismos bits; tambien en el relleno):
     * avail = algun momento con un bebe al que acompañar (esperando o con el);
     * idle  = encendido sin bebe (buscando, fin de estancia o despedida).
     * Dia de descanso = idle y nunca avail ni corazon. Apagado todo el dia no cuenta. */
    uint8_t avail_mask;
    uint8_t idle_mask;
    int32_t gesture_day;    /* ultimo dia con gesto */
    int32_t mask_day;
    int32_t remind_day;     /* ultimo dia con llamada */
    int32_t farewell_day;   /* dia en que empezo la despedida */
    uint8_t n_hist;
    acc_hist_t hist[ACC_HIST_MAX];
} acc_persist_t;

typedef struct {
    char id[TWIN_INCUBATOR_ID_LEN];
    bool present; /* bebe dentro o con sus papas */
    bool home;    /* se ha ido a casa */
} acc_seen_t;

typedef struct {
    acc_persist_t p;
    acc_seen_t seen[ACC_MAX_INC];
    uint8_t n_seen;
    uint32_t seed;          /* hash del client id: reparte los empates entre paneles */
    uint32_t ending_since;  /* s; entrada en TWIN_ACC_ENDING */
    uint32_t pick_after;    /* s; no elegir antes (dejar llegar todos los retenidos) */
    uint32_t unsaved_s;     /* segundos contados desde el ultimo guardado */
} acc_t;

uint32_t acc_hash(const char *s); /* FNV-1a de 32 bits */

/* Estado inicial (buscando, historial vacio). */
void acc_init(acc_t *a, uint32_t seed);
/* Recupera lo guardado; si no es valido, acc_init. */
void acc_restore(acc_t *a, const acc_persist_t *p, uint32_t seed, uint32_t now_s);
/* No elegir hasta now_s >= until_s (al conectar llegan de golpe los retenidos). */
void acc_hold_pick(acc_t *a, uint32_t until_s);
/* Segundos en fin de estancia (0 si no lo esta). El pegamento guarda con esto la hora real en
 * que empezo, para que un reinicio no vuelva a contar las 2 h desde cero. */
uint32_t acc_ending_elapsed(const acc_t *a, uint32_t now_s);
/* Fin de estancia que venia de antes de reiniciar y empezo hace elapsed_s (por la hora real).
 * Solo pasada la espera de los retenidos: si el bebe ha vuelto ya no hay fin de estancia y se
 * descarta. Nunca acorta lo ya contado y no pasa de ACC_ENDING_S. Devuelve false si aun no toca
 * (volver a llamar) y true si se ha aplicado o ya no hace falta. */
bool acc_ending_resume(acc_t *a, uint32_t now_s, uint32_t elapsed_s);

/* Estado recibido de una IncuNest. present = bebe dentro o con sus papas; home = alta. */
uint32_t acc_seen_update(acc_t *a, const char *id, bool present, bool home, uint32_t now_s);
/* Que decir al pulsar los corazones (bit 0 = hoy). n = dias seguidos hasta hoy, o hasta ayer si
 * hoy aun no. */
typedef enum {
    ACC_SM_NONE,   /* ningun corazon: como funciona */
    ACC_SM_START,  /* ni hoy ni ayer: empieza una racha */
    ACC_SM_TODAY1, /* hoy si, ayer no */
    ACC_SM_RUN,    /* n dias seguidos hasta hoy (2..6) */
    ACC_SM_WEEK,   /* los 7 */
    ACC_SM_KEEP1,  /* ayer si, hoy aun no */
    ACC_SM_KEEP,   /* n dias hasta ayer, hoy aun no */
    ACC_SM_REST,   /* hoy (de momento) no hay bebes: la racha de n dias no se rompe */
} acc_streak_msg_t;
/* rest = dias sin bebes (acc_rest_mask): no suman a la racha pero tampoco la rompen */
acc_streak_msg_t acc_streak_msg(uint8_t mask, uint8_t rest, int *n);
/* Dias de descanso (sin bebes en todo el dia y sin corazon), bit 0 = hoy */
uint8_t acc_rest_mask(const acc_t *a, int32_t day);

/* Pais que se puede acompañar: ISO alfa-2 conocido y que no sea España (en España solo hay
 * IncuNest de muestra o de fabrica). Sin pais o "XX" tampoco: podria ser una de aqui. */
bool acc_country_ok(const char *cc);
/* Estado de una IncuNest que no se puede acompañar: nunca se elige y, si es la actual, se deja
 * en el acto (sin despedida ni margen de 2 h) y se busca otra. */
uint32_t acc_exclude(acc_t *a, const char *id, uint32_t now_s);
/* Indice de la IncuNest en a->seen (para el payload guardado en PSRAM); -1 si no esta. */
int acc_seen_index(const acc_t *a, const char *id);

/* "Coge mi mano" completado. */
uint32_t acc_gesture(acc_t *a, int32_t day);
/* 3 minutos de mano en el mismo gesto: super corazon ese dia (tambien cuenta como gesto). */
#define ACC_SUPER_S 180
uint32_t acc_super(acc_t *a, int32_t day);
uint8_t acc_super_mask(const acc_t *a, int32_t day);
/* Toque en la pantalla (cierra la despedida). */
uint32_t acc_touch(acc_t *a, uint32_t now_s);
/* Cada segundo. hour = hora local 0..23, o -1 sin hora valida; shown = bebe a la vista. */
uint32_t acc_tick(acc_t *a, uint32_t now_s, int32_t day, int hour, bool shown);

/* Salidas */
static inline twin_acc_t acc_phase(const acc_t *a) { return (twin_acc_t)a->p.phase; }
static inline const char *acc_current(const acc_t *a) { return a->p.cur; }
uint32_t acc_hist_secs(const acc_t *a, const char *id);
/* "¿Le das la mano hoy?": hay bebe, son las 19:00 o mas y hoy no ha habido gesto. */
bool acc_remind_active(const acc_t *a, int32_t day, int hour);
/* Corazones: bit 0 = hoy ... bit 6 = hace 6 dias. */
uint8_t acc_week_mask(const acc_t *a, int32_t day);

#ifdef __cplusplus
}
#endif
