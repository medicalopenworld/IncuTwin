#pragma once

/* Modo demo por botón BOOT — para enseñar la IncuTwin sin WiFi.
 *
 *   - pulsación larga (DEMO_HOLD_MS) : entra o sale del modo demo
 *   - pulsación corta (ya en demo)   : siguiente escenario
 *   - pulsación corta fuera de demo  : se ignora (no molesta en campo)
 *
 * Mientras está activo:
 *   - los escenarios de scenarios.h se ciclan en orden y en bucle
 *   - se falsea la conectividad del panel (wifi_connected /
 *     cloud_connected) y se refresca last_update_ms cada segundo, para
 *     que la UI se vea "en línea" sin red y no salte DATA_STALE_S
 *   - las fuentes reales (tb_client, wifi_service) no tocan
 *     g_state: consultan demo_is_active()
 *
 * Al salir se restaura el g_state que había al entrar.
 *
 * demo_init()/demo_tick() se llaman desde el hilo de LVGL (setup/loop). */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Habilita el botón. Solo tiene sentido con la UI principal en marcha
 * (panel ya provisionado); en el onboarding no se llama. */
void demo_init(void);

/* Muestrea el botón. Llamar en cada vuelta de loop(). */
void demo_tick(void);

bool demo_is_active(void);

/* Contador que avanza en cada entrada/salida y en cada cambio de
 * escenario. La UI lo vigila para descartar transitorios (la ventana
 * "con sus papás") al saltar de escenario. */
uint32_t demo_seq(void);

#ifdef __cplusplus
}
#endif
