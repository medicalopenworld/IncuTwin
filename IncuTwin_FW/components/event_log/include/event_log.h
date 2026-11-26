/* IncuTwin — historial de los 50 ultimos avisos, persistido en NVS (namespace "evlog").
 *
 * Se alimenta solo: escucha TWIN_EVT_NOTICE (ignora los de la demo) y publica
 * TWIN_EVT_LOG_CHANGED al cambiar. Todas las funciones son seguras entre tareas.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "event_ring.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Carga el historial (anillo vacio si no existe o su tamano no cuadra) y se suscribe a los avisos.
 * Requiere storage_init() y app_events_init() hechos. */
void event_log_init(void);
/* Una entrada por bit de `notices_mask` (twin_notice_t), en orden de bit. `country` puede ser NULL;
 * `sex` es twin_sex_t. */
void event_log_add(uint32_t notices_mask, const char *inc, const char *country, uint8_t sex);
uint8_t event_log_count(void);
bool event_log_get(uint8_t i, event_entry_t *out); /* 0 = la mas reciente */
uint8_t event_log_unseen(void);
void event_log_mark_seen(void);

#ifdef __cplusplus
}
#endif
