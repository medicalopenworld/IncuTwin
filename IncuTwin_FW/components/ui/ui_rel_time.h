/* Tiempo relativo de un aviso del Historial («hace 2 h» / «2 h ago»). Modulo puro (sin LVGL):
 * se compila tambien en test_apps. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* `at` y `now` en segundos epoch; `lang` = SETTINGS_LANG_ES (0) / SETTINGS_LANG_EN (1).
 * Menos de 60 s (o reloj corregido hacia atras) -> «ahora»; luego min, h y d enteros.
 * `at == 0` (el aviso llego sin hora) -> "-" (las fuentes no tienen la raya U+2014). */
void ui_rel_time(uint32_t at, uint32_t now, char *out, size_t n, uint8_t lang);

#ifdef __cplusplus
}
#endif
