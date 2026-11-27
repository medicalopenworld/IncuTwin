/* IncuTwin — caricia y cosquillas a partir del dedo sobre la pantalla. C puro, sin LVGL ni red:
 * los puntos entran en pixeles y el tiempo como parametro (ms monotonos, se tolera el
 * desbordamiento de 32 bits). Lo lleva la tarea de LVGL; probado en test_apps. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rewards_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Valores de partida: se ajustan con la placa. */
#define AFF_STEP_MIN_PX      6    /* un movimiento menor no cuenta: ruido del tactil */
#define AFF_CARESS_PX        40   /* recorrido para caricia */
#define AFF_TURN_MIN_PX      12   /* tramo minimo en el sentido nuevo para contar un cambio */
#define AFF_TICKLE_TURNS     4    /* cambios de sentido ... */
#define AFF_TICKLE_WINDOW_MS 1500 /* ... dentro de esta ventana */

typedef enum { AFF_NONE = 0, AFF_CARESS, AFF_TICKLES } aff_kind_t;

typedef struct {
    bool down;
    int16_t x, y;        /* ultimo punto contado */
    uint16_t path;       /* recorrido acumulado, px */
    int8_t dir;          /* sentido horizontal del ultimo tramo: -1, 0, +1 */
    uint8_t turns;       /* cambios de sentido dentro de la ventana */
    uint32_t turn_ms[4]; /* marcas de los ultimos cambios (la mas antigua primero) */
    aff_kind_t best;     /* lo mas alto reconocido en este toque */
} aff_stroke_t;

/* Lo que abre cada nivel: CARESS desde RW_LEVEL_CARESS, TICKLES desde RW_LEVEL_TICKLES. */
bool aff_open(rw_level_t level, aff_kind_t kind);
/* Dedo puesto: empieza un toque de cero. */
void aff_down(aff_stroke_t *s, int16_t x, int16_t y, uint32_t now_ms);
/* Devuelve lo reconocido hasta ahora en este toque (nunca baja), limitado por el nivel. */
aff_kind_t aff_move(aff_stroke_t *s, int16_t x, int16_t y, uint32_t now_ms, rw_level_t level);
/* Dedo levantado: los movimientos siguientes no cuentan hasta el proximo aff_down. */
void aff_up(aff_stroke_t *s);

#ifdef __cplusplus
}
#endif
