#pragma once

/* CrowPanel Advance 2.8" — pin map (from Elecrow reference example) */

/* Portrait mount (USB on top): the ST7789 is 240x320 native, so portrait
 * is rotation 0/2. If the image comes up upside down on your unit, change
 * offset_rotation in LGFX_CrowPanel28.h (panel and touch) from 0 to 2. */
#define LCD_H_RES 240
#define LCD_V_RES 320

/* ST7789 over SPI */
#define LCD_PIN_SCLK 42
#define LCD_PIN_MOSI 39
#define LCD_PIN_MISO -1
#define LCD_PIN_DC 41
#define LCD_PIN_CS 40
#define LCD_PIN_RST -1

/* Backlight */
#define LCD_PIN_BL 38

/* Capacitive touch (I2C). This exact unit carries an FT5x06/FT6336 at
 * 0x38 (confirmed via I2C bus scan) — Elecrow ships this board with
 * either that chip or a GT911 at 0x5D depending on the batch. */
#define TOUCH_SDA 15
#define TOUCH_SCL 16
#define TOUCH_FT5X06_INT 47
#define TOUCH_FT5X06_ADDR 0x38

/* On-board buzzer (PWM; factory firmware drives it with analogWrite(8, x)) */
#define BUZZER_PIN 8
