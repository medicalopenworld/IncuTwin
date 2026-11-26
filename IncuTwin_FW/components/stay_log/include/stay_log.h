/* IncuTwin — los bebes que han pasado por cada IncuNest (modulo puro, sin NVS ni FreeRTOS).
 *
 * El Historial no sale de los avisos (el registro de avisos solo guarda los 50 ultimos y, en
 * acompañar, la IncuNest que te toca ya tiene el bebe dentro sin aviso de ingreso): se alimenta
 * del estado de la incubadora. Bebe dentro (o con sus papas, o con calor o fototerapia: el
 * cuidado implica bebe) y ninguna estancia abierta en esa IncuNest -> abre una; sin bebe con la
 * IncuNest en linea -> la cierra. Desconectada no se cierra nada (no se sabe). Si ThingsBoard
 * manda stay_id, un stay_id nuevo es otro bebe aunque no se haya visto la salida.
 *
 * Guarda las STAY_LOG_MAX estancias mas recientes (al llenarse pisa la mas antigua) y, por
 * IncuNest, cuantos bebes han pasado: ese contador no baja al pisar el anillo.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "twin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STAY_LOG_MAX 40
#define STAY_INC_MAX 12

typedef struct {
    uint32_t arrived_at; /* epoch; 0 = sin hora (se pone en cuanto la haya, con la estancia abierta) */
    uint32_t left_at;    /* epoch de la salida; 0 = sigue dentro o sin hora */
    uint32_t key;        /* hash del stay_id de ThingsBoard; 0 = no llega */
    char inc[TWIN_INCUBATOR_ID_LEN];
    uint8_t sex;         /* twin_sex_t */
    uint8_t open;        /* 1 = sigue dentro, por lo que sabemos */
} stay_t;

typedef struct {
    char inc[TWIN_INCUBATOR_ID_LEN]; /* "" = hueco libre */
    char country[TWIN_COUNTRY_LEN];
    uint16_t babies;                 /* bebes que han pasado por ella */
    uint32_t last_at;                /* ultimo cambio (para ordenar); 0 = sin hora */
    uint32_t order;                  /* contador de cambios: desempata sin hora */
} stay_inc_t;

typedef struct {
    uint8_t head;  /* proxima posicion a escribir */
    uint8_t count; /* estancias validas */
    uint32_t seq;  /* contador de cambios (stay_inc_t.order) */
    stay_t s[STAY_LOG_MAX];
    stay_inc_t inc[STAY_INC_MAX];
} stay_log_t;

/* Lo que se ve de la IncuNest actual en un estado */
typedef struct {
    const char *inc;     /* numero de serie; "" o NULL = sin IncuNest (no hace nada) */
    const char *country; /* puede ser "" */
    const char *stay_id; /* puede ser "" */
    bool present;        /* hay bebe (dentro, con sus papas o con cuidados activos) */
    bool known;          /* la IncuNest esta en linea y su estado es fiable */
    uint8_t sex;         /* twin_sex_t */
    uint32_t now;        /* epoch; 0 si el reloj aun no esta en hora */
} stay_obs_t;

/* true si algo cambio (hay que guardar) */
bool stay_log_observe(stay_log_t *l, const stay_obs_t *o);

/* Indices de l->inc con datos, del cambio mas reciente al mas antiguo. Devuelve cuantos. */
uint8_t stay_log_incubators(const stay_log_t *l, uint8_t out[STAY_INC_MAX]);

/* Identidad de la estancia abierta de `inc` (hash de la IncuNest y su stay_id o, sin el, su hora de
 * llegada); 0 si no hay ninguna abierta. Para saber a que bebe se dio la mano (rewards). */
uint32_t stay_log_open_key(const stay_log_t *l, const char *inc);

/* Estancias de `inc`, de la mas reciente a la mas antigua (punteros dentro de l). Devuelve cuantas. */
uint8_t stay_log_babies(const stay_log_t *l, const char *inc, const stay_t *out[], uint8_t max);

#ifdef __cplusplus
}
#endif
