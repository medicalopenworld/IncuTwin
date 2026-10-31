/* IncuTwin — contadores de uso de por vida (spec usage-stats), NVS "usage".
 *   on_s   segundos encendido
 *   conn_s segundos con la incubadora conectada (link_ok && online, sin demo)
 *   hand_s segundos tocando al bebe / boton "Agarra mi mano"
 *   hand_n numero de toques
 * Se persisten cada 600 s de encendido. No se publican en esta version. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t on_s, conn_s, hand_s, hand_n;
} usage_stats_t;

void usage_init(void);
void usage_tick_1s(bool incubator_connected); /* desde el timer de 1 s */
void usage_hand_set(bool holding);            /* desde la UI, al tocar/soltar */
usage_stats_t usage_get(void);

#ifdef __cplusplus
}
#endif
