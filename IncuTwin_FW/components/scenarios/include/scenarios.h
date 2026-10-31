/* IncuTwin — escenarios predefinidos del gemelo (spec demo-mode, sim-server).
 * Los usan el modo demo (boton BOOT) y el servidor de simulacion. El orden de la
 * tabla es el del ciclado. */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "twin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *id;
    bool linked;          /* emparejada y con estado recibido */
    twin_incubator_t inc;
} twin_scenario_t;

size_t scenario_count(void);
const twin_scenario_t *scenario_get(size_t idx);      /* NULL fuera de rango */
const twin_scenario_t *scenario_find(const char *id); /* NULL si no existe */

#ifdef __cplusplus
}
#endif
