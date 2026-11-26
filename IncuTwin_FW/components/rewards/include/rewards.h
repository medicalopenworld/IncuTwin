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
/* Compra (con corazones o el regalo) o elige, con la fecha local; el motivo si no se puede
 * (*missing con lo que falta). */
rw_unlock_t rewards_unlock(rw_item_t item, uint16_t *missing);
/* Mes (1..12) y dia locales para la temporada de Navidad; false y 0, 0 sin hora. */
bool rewards_local_md(int *month, int *mday);
/* Pruebas: fuerza la fecha de rewards_local_md (mes 0 = volver a la de verdad). No se guarda. */
void rewards_force_date(int month, int mday);
/* El canje de «Una IncuNest con tu nombre» ya se publico en el broker (no se vuelve a mandar). */
void rewards_mark_redeem_sent(void);
/* IncuPray: la apuesta (gasta `n` si hay saldo) y los corazones ganados (fuera del tope diario).
 * Con la demo activa van al estado simulado. */
bool rewards_spend(uint16_t n);
void rewards_add(uint8_t hearts);
/* Un «Coge mi mano» completado en la demo (suma al estado simulado). */
void rewards_demo_gesture(const char *country);

#ifdef __cplusplus
}
#endif
