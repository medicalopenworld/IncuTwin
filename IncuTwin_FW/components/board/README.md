# board — CrowPanel Advance 2.8" (ESP32-S3-WROOM-1-N16R8)

Único componente que conoce pines y drivers. Pines en `board_pins.h`; orientación en Kconfig
(`menuconfig → IncuTwin board`).

## Valores verificados en hardware (1-oct-2026, unidad ITW-DEEF3C)

| Elemento | Driver | Configuración | Resultado |
|---|---|---|---|
| ST7789 240×320 | `esp_lcd` SPI2, DMA, 80 MHz | `swap_xy=0`, `mirror_x=1`, `mirror_y=1` (180°), `invert_color=1`, RGB | vertical con USB arriba, colores correctos, sin tearing |
| Táctil | `esp_lcd_touch_ft5x06` @0x38 (autodetección; GT911 @0x5D/0x14 como alternativa) | `swap_xy=0`, `mirror_x=1`, `mirror_y=1` (**combo 6**) | las 5 dianas aciertan bajo el dedo |
| Retroiluminación | LEDC timer 0, canal 0, 5 kHz, 8 bits, GPIO 38 | 10/50/100 % | sin parpadeo |
| Zumbador | LEDC timer 1, canal 1, 10 bits, GPIO 8 | escala C5–C6 a duty 512 | suena, el brillo no varía |
| Botón BOOT | GPIO 0, pull-up | activo a nivel bajo | corta/larga distinguibles |
| Memoria tras init | — | — | 273 KB heap interno libre, 8186 KB PSRAM |

LVGL 9 corre vía `esp_lvgl_port` en el core 1 (prio 4, 8 KB) con doble buffer parcial de
240×64 px en RAM interna DMA y `swap_bytes` para el RGB565 big-endian del ST7789.

## Pantalla de bring-up

`idf.py -B build-bringup -D SDKCONFIG=build-bringup/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.bringup" flash`

Dianas táctiles con marcador del último toque, rectángulos de orientación, círculo en movimiento,
rampa de brillo, BOOT corto = siguiente combinación `swap/mirror` del táctil, BOOT largo = escala.
El log serie imprime cada toque con su error respecto a la diana más cercana.
