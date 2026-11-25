/* IncuTwin — anillo de los ultimos avisos (modulo puro, sin NVS ni FreeRTOS).
 *
 * Guarda los EVENT_LOG_MAX avisos mas recientes; al llenarse pisa el mas antiguo.
 * `unseen` cuenta los que aun no ha visto quien mira el historial (tope: EVENT_LOG_MAX).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "twin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EVENT_LOG_MAX 50

typedef struct {
    uint32_t at;                      /* epoch en segundos; 0 si el reloj aun no estaba en hora */
    uint8_t kind;                     /* un solo bit de twin_notice_t */
    char inc[TWIN_INCUBATOR_ID_LEN];  /* incubadora a la que se refiere */
} event_entry_t;

typedef struct {
    uint8_t head;   /* proxima posicion a escribir */
    uint8_t count;  /* entradas validas, 0..EVENT_LOG_MAX */
    uint8_t unseen; /* entradas sin ver, 0..EVENT_LOG_MAX */
    event_entry_t e[EVENT_LOG_MAX];
} event_ring_t;

void event_ring_push(event_ring_t *r, const event_entry_t *e);
/* i = 0 es la mas reciente. false si i >= count. */
bool event_ring_get(const event_ring_t *r, uint8_t i, event_entry_t *out);
void event_ring_mark_seen(event_ring_t *r);

#ifdef __cplusplus
}
#endif
