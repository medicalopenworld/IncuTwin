/* Assets del bebe (gen_assets.py)
 * Generado por tools/ — NO editar a mano. */
#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_image_dsc_t img_baby_sleep;
extern const lv_image_dsc_t img_baby_yawn1;
extern const lv_image_dsc_t img_baby_yawn2;
extern const lv_image_dsc_t img_baby_awake;
extern const lv_image_dsc_t img_baby_smile;
extern const lv_image_dsc_t img_baby_laugh;
extern const lv_image_dsc_t img_baby_girl_sleep;
extern const lv_image_dsc_t img_baby_girl_yawn1;
extern const lv_image_dsc_t img_baby_girl_yawn2;
extern const lv_image_dsc_t img_baby_girl_awake;
extern const lv_image_dsc_t img_baby_girl_smile;
extern const lv_image_dsc_t img_baby_girl_laugh;
extern const lv_image_dsc_t img_baby_santa;
extern const lv_image_dsc_t img_heart;
extern const lv_image_dsc_t img_logo;

#define BABY_SKIN_TONE_COUNT 5
#define BABY_PALETTE_BYTES (256 * 4)
extern const uint8_t baby_skin_palettes[5][256 * 4];
/* Tienda: estilos de gorro y manta (entradas de paleta y sus colores, B,G,R,A) */
#define BABY_HAT_BASIC 0
#define BABY_HAT_MINT 1
#define BABY_HAT_CHICK 2
#define BABY_HAT_CLOUD 3
#define BABY_HAT_LILAC 4
#define BABY_HAT_PEACH 5
#define BABY_HAT_STYLE_COUNT 6
#define BABY_HAT_N_IDX 54
#define BABY_BLANKET_BASIC 0
#define BABY_BLANKET_CREAM 1
#define BABY_BLANKET_MINT 2
#define BABY_BLANKET_LAVENDER 3
#define BABY_BLANKET_PEACH 4
#define BABY_BLANKET_STYLE_COUNT 5
#define BABY_BLANKET_N_IDX 56
extern const uint8_t baby_hat_idx[BABY_HAT_N_IDX];
extern const uint8_t baby_hat_colors[BABY_HAT_STYLE_COUNT][BABY_HAT_N_IDX][4];
extern const uint8_t baby_blanket_idx[BABY_BLANKET_N_IDX];
extern const uint8_t baby_blanket_colors[BABY_BLANKET_STYLE_COUNT][BABY_BLANKET_N_IDX][4];
/* Papa Noel: esquina relativa al sprite del bebe */
#define BABY_SANTA_X (18)
#define BABY_SANTA_Y (-20)

#ifdef __cplusplus
}
#endif
