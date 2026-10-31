/* Contrato interno entre las pantallas del componente ui. Todo corre en la tarea
 * de LVGL salvo que se indique. */
#pragma once

#include "app_events.h"
#include "lvgl.h"
#include "twin_types.h"
#include "ui_i18n.h"
#include "ui_theme.h"

/* Objeto que debe sonar como boton al pulsarlo aunque no derive de lv_button_class
 * (botones del msgbox). Lo lee click_feedback_cb en ui.c. */
#define UI_FLAG_CLICK_SOUND LV_OBJ_FLAG_USER_1

/* pantalla principal */
lv_obj_t *ui_home_create(void);
void ui_home_apply(const twin_snapshot_t *s);
void ui_home_retranslate(void);

/* ajustes */
lv_obj_t *ui_settings_create(void);
void ui_settings_apply(const twin_snapshot_t *s);
void ui_settings_update(const app_evt_settings_t *st);
void ui_settings_retranslate(void);

/* navegacion (ui.c) */
void ui_go_settings(void);
void ui_go_home(void);

/* Dialogo modal con nuestras fuentes (la de LVGL no tiene ¿ ni acentos), 208 px de ancho,
 * boton de cierre y un boton de accion que recibe `user_data`. Devuelve el msgbox. */
lv_obj_t *ui_msgbox(const char *title, const char *text, const char *btn_text, lv_color_t btn_color,
                    lv_event_cb_t btn_cb, void *user_data);
