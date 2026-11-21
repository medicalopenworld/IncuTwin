/* Contornos de tierra (Natural Earth 1:110m) en centesimas de grado; ver tools/gen_map.py. */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint16_t MAP_LAND_RINGS;
extern const uint16_t MAP_LAND_POINTS;
extern const int16_t map_land_pts[][2];       /* lon, lat; anillos seguidos, sin repetir el 1.o */
extern const uint16_t map_land_ring_len[];    /* puntos de cada anillo */
extern const int16_t map_land_ring_lat[][2];  /* latitud min, max de cada anillo */

#ifdef __cplusplus
}
#endif
