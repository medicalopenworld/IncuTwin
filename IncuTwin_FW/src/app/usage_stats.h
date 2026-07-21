#pragma once

/* Contadores de uso persistentes (NVS "usage"), publicados en ThingsBoard:
 *   on_s     — segundos encendido (acumulado de por vida)
 *   conn_s   — segundos con la incubadora conectada
 *   hand_s   — segundos de "Agarra mi mano" (el usuario toca al bebé)
 *   hand_n   — nº de veces que se ha usado "Agarra mi mano"
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void usage_init(void);
void usage_tick_1s(bool incubator_connected); /* llamar cada segundo */
void usage_hand_begin(void);
void usage_hand_end(void);

uint32_t usage_on_s(void);
uint32_t usage_conn_s(void);
uint32_t usage_hand_s(void);
uint32_t usage_hand_n(void);

#ifdef __cplusplus
}
#endif
