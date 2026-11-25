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
void ui_home_screen_pressed(void);
void ui_home_goto_baby(void); /* desde la etiqueta del pais del mapa */
bool ui_home_overlay_active(void); /* despedida o agradecimiento de «Coge mi mano»: sin barra ni franja */
void ui_home_status_refresh(void); /* repinta la barra de estado (la hora del mapa ha cambiado) */ /* toque en cualquier sitio (despedida del modo acompañar) */

/* ajustes */
lv_obj_t *ui_settings_create(void);
void ui_settings_apply(const twin_snapshot_t *s);
void ui_settings_update(const app_evt_settings_t *st);
void ui_settings_retranslate(void);

/* historial (ui_history.c) */
lv_obj_t *ui_history_create(void);
void ui_history_refresh(void); /* relee event_log: solo al cambiar el historial o al abrirlo */
void ui_history_retranslate(void);
/* circulo de icono de un aviso (un bit de twin_notice_t) como hijo de `row`: Historial y franja */
void ui_notice_icon(lv_obj_t *row, uint8_t kind);

/* franja de aviso en pantalla (ui_notice.c, capa superior) */
void ui_notice_init(void);
void ui_notice_push(uint32_t mask); /* encola un aviso por bit, en orden de bit; cola de 8 */
void ui_notice_poll(bool blocked);  /* desde refresh_cb: saca el siguiente si no hay nada que lo tape */

/* navegacion: barra fija de abajo (ui_nav.c) y cambio de pantalla (ui.c) */
typedef enum { UI_TAB_HOME = 0, UI_TAB_HISTORY, UI_TAB_SETTINGS } ui_tab_t;
#define UI_NAV_Y 280 /* nada de las pantallas con barra por debajo de esta linea */
lv_obj_t *ui_nav_create(lv_obj_t *scr, ui_tab_t active);
void ui_nav_set_unread(bool unread); /* punto en el pollito de las tres barras */
/* Desliza 220 ms a la pantalla de `tab` (a la izquierda si su indice sube); al entrar en el
 * Historial marca los avisos como vistos. */
void ui_go(ui_tab_t tab);

/* Dialogo modal con nuestras fuentes (la de LVGL no tiene ¿ ni acentos), 208 px de ancho,
 * boton de cierre y un boton de accion que recibe `user_data`. Devuelve el msgbox. */
lv_obj_t *ui_msgbox(const char *title, const char *text, const char *btn_text, lv_color_t btn_color,
                    lv_event_cb_t btn_cb, void *user_data);
