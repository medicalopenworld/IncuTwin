/* IncuTwin — modelo del gemelo: unico dueno del estado (design.md D4, D9).
 *
 * Recibe el estado del enlace (WiFi, broker, emparejado), los mensajes de
 * incubators/<id>/state y el overlay del modo demo; publica
 * TWIN_EVT_STATE_CHANGED (twin_snapshot_t) y TWIN_EVT_TRANSITION
 * (twin_transition_t). Persiste event_seq y baby en NVS "twin".
 *
 * Todas las funciones son seguras desde cualquier tarea (mutex interno) y no
 * bloquean mas que lo que tarda un post al bus de eventos.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sdkconfig.h"
#include "twin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void twin_model_init(uint8_t default_skin, bool has_creds, const char *incubator_id);

/* enlace */
void twin_model_set_wifi(bool connected, int8_t rssi, const char *ip);
void twin_model_set_rssi(int8_t rssi); /* muestreo periodico sin republicar si no cambia el nivel */
void twin_model_set_broker(bool connected);
void twin_model_set_pairing(const char *incubator_id); /* NULL o "" = desemparejar */

/* mensaje incubators/<id>/state (JSON crudo) */
void twin_model_ingest(const char *json, size_t len);

/* cada segundo: evalua broker_lost */
void twin_model_tick_1s(void);

/* overlay demo (spec demo-mode) */
void twin_model_demo_enter(void);
void twin_model_demo_exit(void);
bool twin_model_demo_active(void);
void twin_model_demo_apply(const twin_incubator_t *inc, bool linked);

twin_snapshot_t twin_model_get(void);

#if CONFIG_INCUTWIN_SIM
/* Solo build de simulacion: finge credenciales, broker y emparejado "SIM", y
 * escribe el estado real directamente (con transiciones). No existe en produccion. */
void twin_model_sim_link(bool linked);
void twin_model_sim_apply(const twin_incubator_t *inc, bool linked);
#endif

#ifdef __cplusplus
}
#endif
