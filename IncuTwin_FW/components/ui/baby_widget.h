/* Bebe animado (spec twin-display, requisito "Bebe animado"):
 *   - respiracion: 3 px arriba/abajo, 2600 ms por sentido
 *   - bostezos: yawn1 320 · yawn2 900 · yawn2 350 · yawn1 320 ms; primero a los 15-25 s,
 *     luego cada 18-40 s
 *   - dormido: ojos cerrados + "z Z z" flotando (14 px, 2400 ms, escalonadas 800 ms)
 *   - corazon que late a bpm reales (minimo 250 ms por latido), oculto con bpm = 0
 *   - 6 tonos de piel por intercambio de paleta (copias en PSRAM)
 * Todo en la tarea de LVGL. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

lv_obj_t *baby_widget_create(lv_obj_t *parent, int32_t x, int32_t y);
void baby_set_skin_tone(uint8_t tone);
uint8_t baby_get_skin_tone(void);
void baby_set_awake(bool awake);
void baby_set_heart_rate(uint16_t bpm); /* 0 oculta el corazon */
lv_obj_t *baby_widget_obj(void);
