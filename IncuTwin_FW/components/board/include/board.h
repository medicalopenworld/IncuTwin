/* IncuTwin — componente board: hardware del CrowPanel Advance 2.8".
 *
 * Inicializa pantalla (esp_lcd ST7789), tactil (esp_lcd_touch, autodetecta
 * FT5x06 o GT911), LVGL (esp_lvgl_port), retroiluminacion, zumbador y boton
 * BOOT. Es el unico componente que conoce pines y drivers de periferico; el
 * resto del firmware solo usa esta API.
 *
 * Hilos: board_init() se llama una vez desde app_main antes de crear la UI.
 * board_backlight_set / board_buzzer_tone / board_button_pressed son seguras
 * desde cualquier tarea (operaciones LEDC/GPIO atomicas).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOARD_TOUCH_NONE = 0,
    BOARD_TOUCH_FT5X06,
    BOARD_TOUCH_GT911,
} board_touch_chip_t;

/* Inicializa todo el hardware y arranca la tarea de LVGL. La retroiluminacion
 * queda al 100 %: settings la ajusta despues al valor persistido. */
esp_err_t board_init(void);

lv_display_t *board_display(void);
lv_indev_t *board_touch_indev(void);           /* NULL si no hay tactil */
esp_lcd_touch_handle_t board_touch_handle(void); /* NULL si no hay tactil */
board_touch_chip_t board_touch_chip(void);
const char *board_touch_chip_name(void);
/* Flags de orientacion del tactil configurados en Kconfig (los de arranque). */
void board_touch_default_flags(bool *swap_xy, bool *mirror_x, bool *mirror_y);
/* Errores I2C acumulados al leer el tactil (diagnostico; nunca abortan). */
uint32_t board_touch_error_count(void);

/* Retroiluminacion 0..100 % (PWM 5 kHz, 8 bits). */
void board_backlight_set(uint8_t percent);
uint8_t board_backlight_get(void);

/* Tono en el zumbador: freq_hz > 0 y duty 1..1023 (10 bits) suena;
 * freq_hz == 0 o duty == 0 silencia. */
void board_buzzer_tone(uint32_t freq_hz, uint16_t duty);

/* Nivel crudo del boton BOOT (true = pulsado). El antirrebote lo hace quien
 * lo consume (demo_mode). */
bool board_button_pressed(void);

/* Imprime heap libre interno y PSRAM con el tag indicado. */
void board_log_memory(const char *tag);

#ifdef __cplusplus
}
#endif
