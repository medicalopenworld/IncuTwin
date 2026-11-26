/* IncuTwin — corazones, sellos, pasaporte y personalizar, persistidos en NVS (namespace "rewards").
 *
 * Pegamento de rewards_logic con la NVS y el bus: escucha TWIN_EVT_ACC_INPUT (gesto completado y
 * super corazon, que la UI solo publica fuera de la demo) y TWIN_EVT_STATE_CHANGED (pais, estancia,
 * «se fue a casa», dias sin bebes), guarda en cuanto algo cambia y publica TWIN_EVT_REWARDS_CHANGED.
 * Con la demo activa hay un estado simulado en RAM que nunca toca el de verdad. Seguro entre tareas.
 */
#pragma once

#include <stdbool.h>

#include "rewards_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Carga y se suscribe. Tras storage_init, app_events_init y stays_init. */
void rewards_init(void);
/* Copia del estado que se ve: el simulado mientras la demo esta activa. */
void rewards_copy(rw_state_t *out, bool *demo);
/* Desbloquea (con corazones) o elige; false si falta saldo (*missing) o no se puede. */
bool rewards_unlock(rw_item_t item, uint16_t *missing);
/* El canje de «Una IncuNest con tu nombre» ya se publico en el broker (no se vuelve a mandar). */
void rewards_mark_redeem_sent(void);
/* Un «Coge mi mano» completado en la demo (suma al estado simulado). */
void rewards_demo_gesture(const char *country);

#ifdef __cplusplus
}
#endif
