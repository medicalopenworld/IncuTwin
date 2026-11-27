/* Bebe sin WiFi (gen_nowifi.py)
 * Generado por tools/ — NO editar a mano. */
#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_image_dsc_t img_nowifi_boy_0;
extern const lv_image_dsc_t img_nowifi_boy_1;
extern const lv_image_dsc_t img_nowifi_boy_2;
extern const lv_image_dsc_t img_nowifi_boy_3;
extern const lv_image_dsc_t img_nowifi_boy_4;
extern const lv_image_dsc_t img_nowifi_girl_0;
extern const lv_image_dsc_t img_nowifi_girl_1;
extern const lv_image_dsc_t img_nowifi_girl_2;
extern const lv_image_dsc_t img_nowifi_girl_3;
extern const lv_image_dsc_t img_nowifi_girl_4;
extern const lv_image_dsc_t img_nowifi_sym0;
extern const lv_image_dsc_t img_nowifi_sym1;
extern const lv_image_dsc_t img_nowifi_sym2;
extern const lv_image_dsc_t img_nowifi_sym3;
extern const lv_image_dsc_t img_nowifi_sym4;
extern const lv_image_dsc_t img_nowifi_sym5;
extern const lv_image_dsc_t img_nowifi_sym6;
extern const lv_image_dsc_t img_nowifi_sym7;
extern const lv_image_dsc_t img_nowifi_sym8;
extern const lv_image_dsc_t img_nowifi_sym9;
extern const lv_image_dsc_t img_nowifi_heart;
extern const lv_image_dsc_t img_nowifi_star;

/* tonos de piel (los de gen_assets / baby_skin_palettes): paleta I8 completa por tono */
#define NOWIFI_SKIN_TONE_COUNT 5
extern const uint8_t nowifi_boy_skin_palettes[5][256 * 4];
extern const uint8_t nowifi_girl_skin_palettes[5][256 * 4];
/* lienzo (px de pantalla) */
#define NOWIFI_W 122
#define NOWIFI_H 229
#define NOWIFI_BODIES 5
#define NOWIFI_SYMS 10
/* esquina de cada cuerpo y de cada paso del simbolo dentro del lienzo */
#define NOWIFI_BOY_BODY_POS { {3, 74}, {3, 74}, {5, 40}, {3, 26}, {0, 39} }
#define NOWIFI_GIRL_BODY_POS { {3, 74}, {3, 74}, {5, 40}, {3, 26}, {0, 39} }
#define NOWIFI_SYM_POS { {74, 9}, {74, 9}, {74, 9}, {74, 9}, {74, 9}, {74, 9}, {74, 9}, {74, 9}, {74, 9}, {74, 9} }
/* punta de la antena del cuerpo 3 (de donde salen las particulas), en el lienzo */
#define NOWIFI_BOY_TIP_X 60
#define NOWIFI_BOY_TIP_Y 30
#define NOWIFI_GIRL_TIP_X 60
#define NOWIFI_GIRL_TIP_Y 30
/* bucle sin WiFi: {cuerpo, simbolo (-1 = ninguno), ms} */
#define NOWIFI_SEQ_LEN 18
#define NOWIFI_SEQ { {0, -1, 700}, {1, -1, 380}, {0, -1, 380}, {1, -1, 380}, {0, -1, 520}, {1, -1, 300}, {2, -1, 480}, {3, -1, 750}, {4, -1, 320}, {4, 0, 220}, {4, 1, 220}, {4, 2, 220}, {4, 3, 380}, {4, 4, 520}, {4, 3, 300}, {4, 4, 520}, {4, 3, 300}, {4, 4, 1400} }
/* asistente: busca senal sin aspa; desde NOWIFI_SEARCH_LOOP se repite */
#define NOWIFI_SEARCH_LEN 9
#define NOWIFI_SEARCH_LOOP 4
#define NOWIFI_SEARCH { {0, -1, 600}, {1, -1, 350}, {2, -1, 450}, {3, -1, 650}, {4, -1, 350}, {4, 0, 260}, {4, 1, 260}, {4, 2, 260}, {4, 3, 700} }
/* celebracion al conectar: {cuerpo, simbolo, ms, salto px} */
#define NOWIFI_CELEB_LEN 27
#define NOWIFI_CELEB { {4, 3, 200, 0}, {4, 5, 110, 0}, {4, 6, 110, 0}, {4, 7, 110, 0}, {4, 8, 140, 0}, {4, 9, 320, 0}, {3, -1, 80, -5}, {3, -1, 80, -10}, {3, -1, 80, -12}, {3, -1, 80, -10}, {3, -1, 80, -5}, {3, -1, 80, 0}, {4, 9, 80, -4}, {4, 9, 80, -7}, {4, 9, 80, -4}, {4, 9, 80, 0}, {3, -1, 80, -5}, {3, -1, 80, -10}, {3, -1, 80, -12}, {3, -1, 80, -10}, {3, -1, 80, -5}, {3, -1, 80, 0}, {4, 9, 80, -4}, {4, 9, 80, -7}, {4, 9, 80, -4}, {4, 9, 80, 0}, {4, 9, 1000, 0} }
#define NOWIFI_CELEB_BURST 1 /* la explosion sale con este salto (desde 1) */
/* particulas: {tipo, vx, vy (px/s), retraso ms}; tipo 0 corazon, 1 estrella, 2.. confeti */
#define NOWIFI_PART_HEART 0
#define NOWIFI_PART_STAR 1
#define NOWIFI_N_PARTS 18
#define NOWIFI_PARTS { {0, -33, -187, 0}, {1, 100, -173, 0}, {0, -124, -124, 40}, {2, 40, -227, 20}, {3, 138, -116, 60}, {1, -139, -80, 80}, {0, 141, -51, 120}, {4, -91, -195, 100}, {5, -135, -36, 140}, {0, 58, -160, 180}, {1, -51, -141, 220}, {2, 182, -105, 200}, {3, -164, -115, 240}, {0, 0, -140, 300}, {5, 138, -197, 160}, {4, 167, -30, 260}, {1, -126, -180, 320}, {2, -159, -14, 300} }
#define NOWIFI_CONFETTI_COLS { 0x6CC3A0, 0xF5A93B, 0x054E92, 0xB79CE0 }
#define NOWIFI_CONFETTI_SIZE 5
#define NOWIFI_G 520
#define NOWIFI_LIFE_MS 1500
#define NOWIFI_FADE_MS 450

#ifdef __cplusplus
}
#endif
