/* Contrato interno entre las pantallas del componente ui. Todo corre en la tarea
 * de LVGL salvo que se indique. */
#pragma once

#include "app_events.h"
#include "lvgl.h"
#include "rewards_logic.h"
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

/* ventana de la actualizacion (ui_ota.c) */
void ui_ota_poll(void);        /* desde el refresco de ui.c */
void ui_ota_note_touch(void);  /* cualquier toque en la pantalla */
void ui_home_screen_pressed(void);
void ui_home_goto_baby(void); /* desde la IncuNest del mapa */
/* consola (ui_debug): 0 pulsa «Coge mi mano», 1 pone el dedo, 2 lo quita, 3 cierra el gesto.
 * false si el boton no estaria a la vista (0 y 1 solo con el bebe en su pagina). */
bool ui_home_debug_hand(int step);
/* consola (ui_debug): cambia de pagina de Inicio como el dedo (UI_PAGE_*) */
void ui_home_debug_page(int page);
bool ui_home_overlay_active(void); /* despedida o agradecimiento de «Coge mi mano»: sin barra ni franja */
bool ui_home_gesture_idle(void);
void ui_home_nowifi_force(int8_t mode); /* consola `sinwifi`: 0 = de verdad, 1 = nino, 2 = nina */
void ui_home_incupray_set(bool open); /* el boton «Coge mi mano» pasa a «IncuPray» mientras esta abierta */
/* IncuPray (ui_incupray.c, spec incupray) */
void ui_incupray_open(void);   /* entra si la ventana esta abierta */
void ui_incupray_poll(void);   /* desde el refresco: avisos, boton de Inicio, lo que queda */
bool ui_incupray_active(void); /* su pantalla esta abierta */   /* «Coge mi mano» en reposo (no a mitad del gesto) */

/* recompensas en la UI (ui_rewards.c): copia del estado, temas, nombres y sellos */
void ui_rewards_refresh(void);           /* relee rewards (al recibir TWIN_EVT_REWARDS_CHANGED) */
const rw_state_t *ui_rewards(void);      /* lo que se ve (el de la demo mientras esta activa) */
lv_color_t ui_theme_bg(void);            /* fondo de Inicio elegido */
lv_color_t ui_theme_halo_calm(void);     /* halo en reposo elegido */
lv_color_t ui_item_color(rw_item_t item);
const char *ui_level_name(rw_level_t level);
const char *ui_seal_name(rw_seal_t seal);
const char *ui_seal_desc(rw_seal_t seal);
const char *ui_item_name(rw_item_t item);
uint8_t ui_hat_style(rw_item_t item);     /* BABY_HAT_* del gorro (Papa Noel: el Basico) */
uint8_t ui_blanket_style(rw_item_t item); /* BABY_BLANKET_* de la manta */
void ui_outfit_apply(void);               /* gorro, manta y Papa Noel del bebe de Inicio (con la fecha) */
lv_obj_t *ui_shop_create(void);           /* pantalla Tienda (ui_shop.c) */
void ui_shop_open(void);
void ui_shop_refresh(void);               /* tras ui_rewards_refresh o al cambiar de idioma */
bool ui_shop_visible(void);
void ui_shop_debug(int tab, bool girl, int try_item, bool buy); /* consola: ui_debug.c */
void ui_shop_special_clicked(void); /* «Una IncuNest con tu nombre» desde Corazones */
lv_obj_t *ui_hearts_screen(void);         /* la pantalla Corazones (la tienda va encima) */
lv_obj_t *ui_seal_icon(lv_obj_t *parent, rw_seal_t seal, bool earned, int32_t d);
void ui_home_rewards_apply(void);        /* tema, guirnalda y lo que dependa de las recompensas */

/* corazon que vuela a la racha al completar el primer «Coge mi mano» del dia (ui_streak_fx.c) */
void ui_streak_fx_create(lv_obj_t *scr);
/* `week`, `super`, `rest`: mascaras de antes del gesto (bit 0 = hoy, sin marcar). Sin hora no hace
 * nada. (x, y): de donde sale el corazon; `hearts`: el «+N» que lleva (0 = ninguno). */
void ui_streak_fx_play(uint8_t week, uint8_t super, uint8_t rest, int32_t x, int32_t y, uint8_t hearts);
void ui_streak_fx_bonus(uint8_t n); /* «+N» que sube (el super corazon) */
void ui_streak_fx_hide(void);
/* Corazon de la semana de la racha: lleno (coral), super (dorado), dia sin bebes (azul claro) o
 * apagado; los que aun no han llegado, casi transparentes. Pagina de la racha y ui_streak_fx. */
void ui_week_heart_style(lv_obj_t *h, bool on, bool super, bool rest, bool future);

/* ajustes */
lv_obj_t *ui_settings_create(void);
void ui_settings_wifi_dialog(void); /* el boton WiFi de Ajustes: estado de la red y cambiarla */

/* Corazones (ui_hearts.c): saldo y nivel, racha, sellos, pasaporte y tienda */
lv_obj_t *ui_hearts_create(void);
void ui_hearts_refresh(void);  /* tras ui_rewards_refresh o al cambiar de idioma */
void ui_hearts_show_top(void); /* al entrar desde la franja de un sello */
void ui_settings_apply(const twin_snapshot_t *s);
void ui_settings_update(const app_evt_settings_t *st);
void ui_settings_retranslate(void);

/* notificaciones (ui_notices.c): todos los cambios de estado, del mas reciente al mas antiguo */
lv_obj_t *ui_notices_create(void);
void ui_notices_refresh(void); /* relee event_log: solo al cambiar o al abrirla */
void ui_notices_retranslate(void);
/* circulo de icono de un aviso (un bit de twin_notice_t) como hijo de `row`: notificaciones y franja.
 * `sex` (twin_sex_t) elige el bebe nino o nina de los avisos de entrada y salida. */
void ui_notice_icon(lv_obj_t *row, uint8_t kind, uint8_t sex);

/* historial (ui_history.c): las IncuNest con cuantos bebes han pasado; tocando una, sus bebes */
lv_obj_t *ui_history_create(void);
void ui_history_refresh(void); /* relee stays: solo al cambiar o al abrirlo (y cada minuto) */
void ui_history_retranslate(void);
void ui_history_reset(void);   /* vuelve a la lista de IncuNest (al entrar desde la barra) */
void ui_history_apply(const twin_snapshot_t *s); /* que IncuNest es la de ahora («Ahora en la IncuNest») */

/* franja de aviso en pantalla (ui_notice.c, capa superior) */
void ui_notice_init(void);
void ui_notice_push(uint32_t mask, uint8_t sex); /* encola un aviso por bit, en orden de bit; cola de 8 */
/* encola los sellos nuevos, la subida de nivel (`level` = el nuevo) y la guirnalda (spec rewards) */
void ui_notice_push_reward(uint8_t new_seals, bool level_up, uint8_t level, bool garland);
/* Tarjeta de subida de nivel (Caricia, Cosquillitas): entra en la misma cola que los avisos y espera
 * a que no haya nada que la tape (un "Coge mi mano" en marcha). `catchup`: el aviso pendiente de arranque. */
void ui_level_card_show(rw_level_t level, bool catchup);
/* Aviso de texto (IncuPray): tocarlo abre la IncuPray si opens_incupray, si no Inicio */
void ui_notice_push_text(const char *txt, bool opens_incupray);
bool ui_notice_layer_busy(void);   /* hay un dialogo u otro objeto visible en la capa superior */
void ui_notice_poll(bool blocked);  /* desde refresh_cb: saca el siguiente si no hay nada que lo tape */

/* navegacion: barra fija de abajo (ui_nav.c) y cambio de pantalla (ui.c) */
typedef enum { UI_TAB_HOME = 0, UI_TAB_NOTICES, UI_TAB_HEARTS, UI_TAB_HISTORY, UI_TAB_SETTINGS, UI_TAB_COUNT } ui_tab_t;
#define UI_NAV_Y 280 /* nada de las pantallas con barra por debajo de esta linea */
/* Una sola barra, en la capa superior: no se mueve al cambiar de pantalla (ui_nav.c) */
void ui_nav_create(void);
void ui_nav_set_active(ui_tab_t active); /* el icono de la pantalla actual, opaco */
void ui_nav_set_visible(bool visible);
lv_obj_t *ui_nav_obj(void);
void ui_nav_set_unread(bool unread); /* punto en el sobre (Notificaciones) */
/* Ensena la barra solo en nuestras pantallas y fuera de la despedida y del final del gesto (ui.c) */
void ui_nav_update(void);
/* Desliza 220 ms a la pantalla de `tab` (a la izquierda si su indice sube); al entrar en
 * Notificaciones marca los avisos como vistos. */
void ui_go(ui_tab_t tab);
/* Telemetria `screen` (filtrada): id = UI_TAB_* o los paneles extra de Inicio */
#define UI_TEL_MAP    10
#define UI_TEL_STREAK 11
#define UI_TEL_SHOP   12
void ui_tel_screen(int id);
/* Id de telemetria del panel que ensena Inicio ahora: UI_TAB_HOME (bebe), UI_TEL_MAP o UI_TEL_STREAK */
int ui_tel_home_id(void);

/* Dialogo modal con nuestras fuentes (la de LVGL no tiene ¿ ni acentos), 208 px de ancho,
 * boton de cierre y un boton de accion que recibe `user_data`. Devuelve el msgbox. */
lv_obj_t *ui_msgbox(const char *title, const char *text, const char *btn_text, lv_color_t btn_color,
                    lv_event_cb_t btn_cb, void *user_data);
