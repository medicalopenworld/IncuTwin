/* Mapa del mundo pintado en el panel (spec twin-display, pagina del pais): encuadre automatico
 * para que se vean a la vez tu IncuTwin y la IncuNest, y relleno de los contornos de tierra con
 * suavizado en un buffer RGB565. Modulo puro (sin LVGL), probado en test_apps. */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Proyeccion equirectangular local: x = (lon - lon0) * sx, y = (lat0 - lat) * sy.
 * sx = sy * cos(latitud media), para que el pais no salga estirado. */
typedef struct {
    float lon0, lat0; /* esquina superior izquierda del buffer */
    float sx, sy;     /* pixeles por grado */
} wm_view_t;

/* Hueco del buffer: arriba y abajo quedan bandas para la informacion que va encima del mapa;
 * los dos puntos se colocan en el centro de lo que queda, a `margin` px de los bordes. */
typedef struct {
    int16_t w, h, top, bottom, margin;
    float min_ppd, max_ppd; /* limites del zoom en pixeles por grado de latitud */
} wm_frame_t;

void wm_fit(wm_view_t *v, const wm_frame_t *f, float lat1, float lon1, float lat2, float lon2);

void wm_project(const wm_view_t *v, float lat, float lon, float *x, float *y);

/* Pinta w x h pixeles RGB565 (stride = w): tierra `land`, mar `sea`, costa suavizada. */
void wm_render(uint16_t *buf, int w, int h, const wm_view_t *v, uint16_t land, uint16_t sea);

#ifdef __cplusplus
}
#endif
