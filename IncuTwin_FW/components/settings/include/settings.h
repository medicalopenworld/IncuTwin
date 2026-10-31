/* IncuTwin — preferencias del panel (spec settings): idioma, volumen, brillo.
 * Persisten en NVS "settings"; valores ausentes o fuera de rango se sustituyen
 * por los de defecto (ES, Alto, 100 %). Cada cambio publica
 * TWIN_EVT_SETTINGS_CHANGED y el brillo se aplica al instante en board.
 * Seguras desde cualquier tarea. */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_LANG_ES 0
#define SETTINGS_LANG_EN 1
#define SETTINGS_VOL_MAX 3
#define SETTINGS_BRIGHT_MIN 10
#define SETTINGS_BRIGHT_MAX 100

void settings_init(void); /* carga, sanea y aplica el brillo */

uint8_t settings_lang(void);
void settings_set_lang(uint8_t lang);

uint8_t settings_volume(void); /* 0 apagado .. 3 alto */
void settings_set_volume(uint8_t level);

uint8_t settings_brightness(void); /* 10..100 */
void settings_set_brightness(uint8_t percent);

#ifdef __cplusplus
}
#endif
