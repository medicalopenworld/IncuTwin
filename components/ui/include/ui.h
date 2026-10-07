/* IncuTwin — interfaz LVGL del gemelo (specs twin-display, settings).
 *
 * ui_init() construye splash, pantalla principal y ajustes, y se suscribe a
 * TWIN_EVT_STATE_CHANGED / TWIN_EVT_SETTINGS_CHANGED. Los handlers solo copian
 * el dato; un lv_timer de 50 ms lo aplica en la tarea de LVGL (nunca se toca
 * LVGL desde el bus de eventos).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void ui_init(void);

#ifdef __cplusplus
}
#endif
