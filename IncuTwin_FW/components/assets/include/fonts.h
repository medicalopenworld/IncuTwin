/* Fuentes de la app (estilo «IncuTwin de bolsillo»), con glifos del espanol y simbolos FontAwesome
 * (tools/gen_fonts.py): lv_font_es_* = Nunito Bold (texto), lv_font_title_* = Fredoka SemiBold
 * (titulos, botones, nombres y numeros). */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

LV_FONT_DECLARE(lv_font_es_12)
LV_FONT_DECLARE(lv_font_es_14)
LV_FONT_DECLARE(lv_font_es_16)
LV_FONT_DECLARE(lv_font_es_20)
LV_FONT_DECLARE(lv_font_title_12)
LV_FONT_DECLARE(lv_font_title_14)
LV_FONT_DECLARE(lv_font_title_16)
LV_FONT_DECLARE(lv_font_title_20)
LV_FONT_DECLARE(lv_font_title_24)

#ifdef __cplusplus
}
#endif
