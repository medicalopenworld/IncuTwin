/* IncuTwin — pines del CrowPanel Advance 2.8" (ESP32-S3-WROOM-1-N16R8).
 *
 * UNICO sitio del firmware donde aparecen numeros de GPIO. Fuente: ejemplo de
 * referencia de Elecrow y la configuracion Arduino anterior, verificada en
 * hardware.
 */
#pragma once

/* ST7789 240x320 por SPI (SPI2 / FSPI via GPIO matrix) */
#define BOARD_LCD_SPI_HOST  SPI2_HOST
#define BOARD_LCD_PIN_SCLK  42
#define BOARD_LCD_PIN_MOSI  39
#define BOARD_LCD_PIN_MISO  (-1)
#define BOARD_LCD_PIN_DC    41
#define BOARD_LCD_PIN_CS    40
#define BOARD_LCD_PIN_RST   (-1)
#define BOARD_LCD_H_RES     240   /* ancho logico en vertical  */
#define BOARD_LCD_V_RES     320   /* alto logico en vertical   */

/* Retroiluminacion (PWM) */
#define BOARD_PIN_BACKLIGHT 38

/* Tactil capacitivo por I2C: FT5x06 @0x38 o GT911 @0x5D segun lote */
#define BOARD_TOUCH_I2C_PORT 0
#define BOARD_TOUCH_PIN_SDA  15
#define BOARD_TOUCH_PIN_SCL  16
#define BOARD_TOUCH_PIN_INT  47
#define BOARD_TOUCH_PIN_RST  (-1)

/* Zumbador (PWM) */
#define BOARD_PIN_BUZZER 8

/* Boton BOOT (strapping, activo a nivel bajo con pull-up interno) */
#define BOARD_PIN_BUTTON 0
