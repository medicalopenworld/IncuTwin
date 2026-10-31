#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_img_dsc_t img_baby_sleep;
extern const lv_img_dsc_t img_baby_yawn1;
extern const lv_img_dsc_t img_baby_yawn2;
extern const lv_img_dsc_t img_baby_awake;
extern const lv_img_dsc_t img_heart;
extern const lv_img_dsc_t img_logo;
extern const lv_img_dsc_t img_wordmark;
extern const lv_img_dsc_t img_icon_thermo;
extern const lv_img_dsc_t img_icon_lamp;
extern const lv_img_dsc_t img_wifi_off;
extern const lv_img_dsc_t img_wifi_0;
extern const lv_img_dsc_t img_wifi_1;
extern const lv_img_dsc_t img_wifi_2;
extern const lv_img_dsc_t img_wifi_3;
extern const lv_img_dsc_t img_incunest_empty;
extern const lv_img_dsc_t img_baby_parents;

#define BABY_SKIN_TONE_COUNT 6
extern const uint8_t baby_skin_palettes[6][256 * 4];

#ifdef __cplusplus
}
#endif
