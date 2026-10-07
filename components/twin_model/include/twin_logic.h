/* IncuTwin — logica pura del gemelo (spec twin-state-ingest, twin-display).
 *
 * Sin estado global, sin IDF, sin LVGL: todo lo que hay aqui se prueba con Unity
 * en test_apps. twin_model.c es el pegamento (mutex, eventos, NVS).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "twin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ parser */

typedef enum {
    TWIN_MSG_STATE_FREE = 0,
    TWIN_MSG_STATE_BABY,
    TWIN_MSG_STATE_OFFLINE,
} twin_msg_state_t;

/* Mensaje incubators/<id>/state ya validado. has_* = clave presente y valida. */
typedef struct {
    char incubator_id[TWIN_INCUBATOR_ID_LEN];
    twin_msg_state_t state;
    bool heat, photo, pulseox;
    int bpm;               /* -1 si null/ausente/invalido */
    bool has_seq;
    uint32_t seq;
    char last_event[24];   /* "" si ausente */
    /* extensiones */
    bool has_thermo;  twin_thermo_t thermo;
    bool has_baby;    twin_baby_t baby;
    bool has_awake;   bool awake;
    bool has_skin;    uint8_t skin;
    bool has_name;    char name[TWIN_NAME_LEN];
    bool has_weight;  uint16_t weight_g;
    bool has_age;     int16_t age_d;
} twin_msg_t;

typedef enum {
    TWIN_PARSE_OK = 0,
    TWIN_PARSE_TOO_BIG,     /* > 1024 bytes */
    TWIN_PARSE_BAD_JSON,
    TWIN_PARSE_NOT_OBJECT,
    TWIN_PARSE_BAD_ID,      /* incubator_id ausente o distinto del esperado */
    TWIN_PARSE_BAD_STATE,   /* state ausente o fuera de {free, baby, offline} */
} twin_parse_result_t;

#define TWIN_PARSE_MAX_LEN 1024

/* expected_id: incubadora emparejada ("" = aceptar cualquiera, solo para el sim). */
twin_parse_result_t twin_parse(const char *json, size_t len, const char *expected_id,
                               twin_msg_t *out);
const char *twin_parse_result_str(twin_parse_result_t r);

/* ------------------------------------------------------------------- apply */

typedef struct {
    bool is_new;           /* seq ausente o mayor que el ultimo visto */
    uint32_t transitions;  /* mascara de twin_transition_t (solo si is_new) */
    bool persist;          /* hay que guardar seq/baby en NVS */
} twin_apply_result_t;

/* Aplica el mensaje a `inc`; actualiza *last_seq si procede. */
twin_apply_result_t twin_apply(twin_incubator_t *inc, uint32_t *last_seq, const twin_msg_t *m);

/* Estado inicial de la incubadora (sin emparejar / tras desemparejar). */
void twin_incubator_reset(twin_incubator_t *inc, uint8_t default_skin);

/* Transiciones al pasar de `before` a `after`. */
uint32_t twin_transitions_between(twin_baby_t before, twin_baby_t after);

/* ------------------------------------------------------------ presentation */

/* Rellena link_ok, show_baby y parents_view. */
void twin_derive(twin_snapshot_t *s);

/* Prioridad de la barra de estado (tabla de 14 filas de la spec). */
twin_status_t twin_status_of(const twin_snapshot_t *s);

/* Nivel de cobertura 0..3 a partir del RSSI (spec wifi-connectivity). */
uint8_t twin_rssi_level(int8_t rssi);

/* true si el id tiene 1..16 caracteres de [A-Za-z0-9_-]. */
bool twin_incubator_id_valid(const char *id);

#ifdef __cplusplus
}
#endif
