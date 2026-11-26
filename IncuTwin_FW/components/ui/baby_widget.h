/* Bebe animado (spec twin-display, requisito "Bebe animado"):
 *   - respiracion: 3 px arriba/abajo, 2600 ms por sentido
 *   - bostezos: yawn1 320 · yawn2 900 · yawn2 350 · yawn1 320 ms; primero a los 15-25 s,
 *     luego cada 18-40 s
 *   - dormido: ojos cerrados + "z Z z" flotando (14 px, 2400 ms, escalonadas 800 ms)
 *   - corazon que late a bpm reales (minimo 250 ms por latido), oculto con bpm = 0
 *   - 6 tonos de piel por intercambio de paleta (copias en PSRAM)
 *   - dibujo de nino o de nina (gorro y manta rosas, lazo, pestanas), mismos fotogramas
 *   - gorro y manta de la tienda por paleta; Papa Noel como sprite hijo
 * Todo en la tarea de LVGL. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

lv_obj_t *baby_widget_create(lv_obj_t *parent, int32_t x, int32_t y);
void baby_set_skin_tone(uint8_t tone);
uint8_t baby_get_skin_tone(void);
void baby_set_awake(bool awake);
void baby_set_girl(bool girl); /* false = nino (tambien con sexo desconocido) */
void baby_set_heart_rate(uint16_t bpm); /* 0 oculta el corazon */
/* Tienda: estilos BABY_HAT_* del gorro de los niños y de las niñas, BABY_BLANKET_* de la manta
 * y el gorro de Papa Noel encima (se aplica con el tono de piel, en las copias de PSRAM). */
void baby_set_outfit(uint8_t hat_boy, uint8_t hat_girl, uint8_t blanket, bool santa);
lv_obj_t *baby_widget_obj(void);
/* Un bebe quieto (dormido) con ese tono y esa ropa, con su propia copia en PSRAM (se libera al
 * borrarlo): para Probar en la tienda sin tocar el de Inicio. `scale` 256 = tamaño real. */
lv_obj_t *baby_preview_create(lv_obj_t *parent, bool girl, uint8_t tone, uint8_t hat, uint8_t blanket, bool santa,
                              uint16_t scale); /* scale: LV_SCALE_NONE o reducido desde la esquina */
