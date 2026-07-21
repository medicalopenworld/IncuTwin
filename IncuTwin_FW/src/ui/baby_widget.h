#pragma once

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

/* Animated baby avatar:
 *   - breathing (gentle zoom)
 *   - random yawns (frame sequence)
 *   - sleeping (closed eyes + floating Zzz) / awake (open eyes)
 *   - beating heart at the configured heart rate
 *   - runtime-configurable skin tone (palette swap, 6 tones)
 */

#ifdef __cplusplus
extern "C" {
#endif

void baby_widget_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y);

void baby_set_skin_tone(uint8_t tone); /* 0..BABY_SKIN_TONE_COUNT-1 */
uint8_t baby_get_skin_tone(void);
void baby_set_awake(bool awake);
void baby_set_heart_rate(uint16_t bpm); /* 0 hides the heart */
void baby_trigger_yawn(void);

lv_obj_t *baby_widget_obj(void);

#ifdef __cplusplus
}
#endif
