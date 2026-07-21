#pragma once

/* Builds all screens (splash -> home -> settings) and keeps them in sync
 * with g_state via an LVGL timer. Call ui_init() once after lv_init(). */

#ifdef __cplusplus
extern "C" {
#endif

void ui_init(void);

#ifdef __cplusplus
}
#endif
