/* IncuTwin — modo demo con el boton BOOT (spec demo-mode).
 *   larga (>= 2000 ms, salta sin soltar): entra/sale de la demo (+ pitido)
 *   corta en demo: siguiente escenario; corta fuera de demo: nada
 * Muestreo del boton cada 10 ms por esp_timer con antirrebote de 30 ms.
 * Solo se arranca con la UI principal (no en el onboarding). */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void demo_mode_start(void);

#ifdef __cplusplus
}
#endif
