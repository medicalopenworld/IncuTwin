/* Avisos de lo que vive el bebe: compara el estado nuevo con el anterior y dice que ha cambiado
 * (ingreso, padres, calor, fototerapia, salida). Modulo puro (sin LVGL ni red), probado en
 * test_apps. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "twin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Lo ultimo que se vio de la incubadora, contra lo que se compara el estado nuevo. */
typedef struct {
    bool valid;           /* ya hay una base contra la que comparar */
    bool treatments_known; /* calor y fototerapia de la base son reales (no supuestos) */
    twin_baby_t baby;
    bool heat;            /* thermo != TWIN_THERMO_OFF */
    bool photo;
} twin_notice_base_t;

/* Base al arrancar: con bebe guardado se compara ya, pero los tratamientos no se conocen aun
 * (no se avisa de calor ni fototerapia por lo que ya estaba encendido). */
void twin_notice_base_boot(twin_notice_base_t *b, bool has_baby, twin_baby_t baby);

/* Actualiza la base con el estado nuevo y devuelve la mascara de twin_notice_t. */
uint32_t twin_notices_update(twin_notice_base_t *b, const twin_incubator_t *now);

#ifdef __cplusplus
}
#endif
