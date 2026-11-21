/* Datos por pais de la IncuNest (ISO 3166-1 alfa-2): posicion aproximada para el mapa, hora
 * local y tono de piel por defecto del bebe. Modulo puro (sin LVGL ni red), probado en
 * test_apps. A la escala del mapa del panel (1,7 grados por pixel) el centro del pais basta. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tono de piel (0..4) si no se sabe el pais: 4 de 5 (decision de Pablo, 21-nov-2026) */
#define TWIN_SKIN_UNKNOWN 3

/* Donde se pinta la IncuTwin si no se conoce su ubicacion: Madrid */
#define TWIN_GEO_HOME_LAT 40.4f
#define TWIN_GEO_HOME_LON (-3.7f)

typedef struct {
    char iso[3];
    float lat, lon;     /* grados; centro aproximado del pais (zona poblada) */
    int16_t utc_min;    /* hora estandar, sin horario de verano */
    uint8_t skin;       /* 0..4 */
} twin_geo_t;

/* Fila del pais o NULL si no esta en la tabla (o iso vacio / "XX"). */
const twin_geo_t *twin_geo_find(const char *iso);

/* Tono por defecto para el pais; TWIN_SKIN_UNKNOWN si no se conoce. */
uint8_t twin_geo_skin(const char *iso);

/* Distancia de circulo maximo en km. */
uint32_t twin_geo_distance_km(float lat1, float lon1, float lat2, float lon2);

#ifdef __cplusplus
}
#endif
