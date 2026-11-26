/* IncuTwin — modo demo con el boton BOOT (spec demo-mode).
 *   larga (>= 2000 ms, salta sin soltar): entra/sale de la demo (+ pitido)
 *   corta en demo: siguiente escenario; corta fuera de demo: nada
 * Muestreo del boton cada 10 ms por esp_timer con antirrebote de 30 ms.
 * Solo se arranca con la UI principal (no en el onboarding). */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void demo_mode_start(void);

/* Entra (true) o sale (false) de la demo registrando `demo on/off` en la telemetria. Sin cambio de
 * estado no hace nada. Tambien la usa la consola. */
void demo_mode_set(bool on);

#ifdef __cplusplus
}
#endif
