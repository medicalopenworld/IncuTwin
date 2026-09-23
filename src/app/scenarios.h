#pragma once

/* Escenarios predefinidos del gemelo: combinaciones de g_state que
 * representan los estados que la IncuTwin sabe mostrar (bebé dormido,
 * alarma, fototerapia, incubadora vacía...).
 *
 * Los usan dos consumidores:
 *   - sim_server (build _sim): botones del panel web y demo automática
 *   - demo_mode  (todos los builds): ciclado con el botón BOOT
 *
 * Aplicar un escenario NO toca la conectividad del panel (wifi_connected
 * / cloud_connected): eso es de quien lo llama. */

#include <stddef.h>

#include "app/app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *id;
    bool linked; /* node_seen        */
    bool online; /* incubator_online */
    thermo_state_t thermo;
    bool photo;
    uint16_t hr;
    bool awake;
    baby_state_t baby;
    bool home; /* con BABY_OUT: alta a casa */
} scenario_t;

size_t scenario_count(void);
const char *scenario_id(size_t idx); /* NULL si idx fuera de rango */

/* Ambas escriben g_state y marcan g_state_dirty.
 * Llamar SIN el lock cogido. */
void scenario_apply_idx(size_t idx);
bool scenario_apply(const char *id); /* false si el id no existe */

#ifdef __cplusplus
}
#endif
