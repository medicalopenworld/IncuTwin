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
 * estado no hace nada. Es el unico camino para entrar y salir (boton, consola, IncuPray de la demo,
 * restablecimiento de fabrica). */
void demo_mode_set(bool on);

/* Como 2 s del boton BOOT (consola `demo on|off`): demo_mode_set + primer escenario al entrar +
 * pitido. Sin cambio de estado no hace nada. */
void demo_mode_button(bool on);

/* Restablecimiento de fabrica: sale de la demo, registra `reset`, envia y borra (no vuelve). */
void demo_mode_factory_reset(void);

#ifdef __cplusplus
}
#endif
