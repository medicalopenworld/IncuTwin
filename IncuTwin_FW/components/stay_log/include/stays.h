/* IncuTwin — bebes que han pasado por cada IncuNest, persistido en NVS (namespace "stays").
 *
 * Pegamento de stay_log con la NVS y el bus: escucha TWIN_EVT_STATE_CHANGED (fuera de la demo),
 * guarda solo cuando algo cambia y publica TWIN_EVT_STAYS_CHANGED. Seguro entre tareas.
 */
#pragma once

#include "stay_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Carga (vacio si no existe o es de otro tamano) y se suscribe. Tras storage_init y app_events_init. */
void stays_init(void);
/* Copia de todo el registro (para pintar el Historial). */
void stays_copy(stay_log_t *out);
/* stay_log_open_key bajo el mutex */
uint32_t stays_open_key(const char *inc);

#ifdef __cplusplus
}
#endif
