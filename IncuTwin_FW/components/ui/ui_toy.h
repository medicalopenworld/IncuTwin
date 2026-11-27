/* Piezas del estilo «IncuTwin de bolsillo» (el de la app, incutwin-app/docs/diseno-app.md):
 * borde tinta de 2 px, sombra dura sin difuminar, esquinas pequeñas, hundirse al pulsar y el
 * corazon de pixeles de 9 x 8. Todo corre en la tarea de LVGL. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TOY_BORDER   2
#define TOY_SHADOW_S 2  /* etiquetas, iconos redondos, fichas pequeñas */
#define TOY_SHADOW_M 3  /* tarjetas y botones */
#define TOY_SINK     2  /* lo que baja una pieza al pulsarla */
#define TOY_R_CARD   12
#define TOY_R_BTN    12
#define TOY_R_PILL   10

/* Sombra dura: un bloque tinta desplazado `px` a la derecha y abajo, con las esquinas de la pieza.
 * Si la pieza se puede pulsar, al pulsarla baja TOY_SINK hacia su sombra (la sombra se queda
 * quieta). Con px = 0 la quita. */
void toy_shadow(lv_obj_t *o, int32_t px);

/* Caja de juguete: fondo crema, borde tinta, esquinas `radius` y sombra dura `shadow`. */
void toy_box(lv_obj_t *o, int32_t radius, int32_t shadow);

/* Boton: principal = coral con texto blanco; si no, crema con texto tinta. Borde, sombra 3 y la
 * letra de los botones (Fredoka) en sus etiquetas hijas. */
void toy_button(lv_obj_t *b, bool primary);

/* Etiqueta (pais, nombre, «Elegido»...): crema, borde, esquinas 10, sombra 2. */
void toy_pill(lv_obj_t *o);

/* Pieza de un selector (pestañas de la tienda, idioma): la elegida rellena de tinta y con texto
 * blanco; las demas crema con texto tinta. `lbl` es su etiqueta (o NULL). */
void toy_segment(lv_obj_t *b, lv_obj_t *lbl, bool on);

/* Corazon de pixeles (cuadricula 9 x 8 de la app): imagen ARGB de 9*cell x 8*cell px, hecha una
 * vez por tipo y tamaño (en PSRAM; sin PSRAM, en la RAM normal) y que no se libera. */
typedef enum {
    TOY_HEART_FULL = 0, /* coral */
    TOY_HEART_EMPTY,    /* contorno apagado, dentro crema */
    TOY_HEART_TODAY,    /* contorno coral, dentro crema */
    TOY_HEART_SUPER,    /* dorado: super corazon */
    TOY_HEART_REST,     /* azul claro: dia sin bebes */
    TOY_HEART_WHITE,    /* blanco (sobre coral) */
    TOY_HEART_COUNT
} toy_heart_t;

const lv_image_dsc_t *toy_heart_dsc(toy_heart_t kind, int32_t cell);
lv_obj_t *toy_heart_img(lv_obj_t *parent, toy_heart_t kind, int32_t cell);

#ifdef __cplusplus
}
#endif
