/* Barra fija de navegacion (spec hmi-navegacion-avisos): 240x40 en y = 280, abajo de Inicio,
 * Notificaciones, Corazones, Historial y Ajustes. Cinco botones de 48x40: casa, sobre (con un punto
 * coral si hay avisos sin ver), corazon, pollito (los bebes que han pasado) y rueda dentada. El de la
 * pantalla en
 * la que se esta, opaco y sobre una pastilla rosa; los otros, al 50 % (estilo de la app: fondo
 * crema y raya tinta arriba). Pulsacion larga (1 s, la del indev) en Ajustes: borrado
 * de fabrica.
 * Una sola barra en la capa superior de LVGL: al cambiar de pantalla las pantallas se deslizan por
 * debajo y la barra se queda quieta. Se esconde fuera de nuestras pantallas (splash, cambio de WiFi)
 * y en la despedida y el final de «Coge mi mano» (ui_nav_update). Los dialogos y el teclado del
 * nombre se crean despues, tambien en la capa superior, y la tapan. */
#include <stdio.h>

#include "assets.h"
#include "esp_log.h"
#include "demo_mode.h"
#include "ui_internal.h"
#include "ui_toy.h"

static const char *TAG = "nav";

#define NAV_H     40
#define NAV_BTN_W 48
#define DOT_D     8

static lv_obj_t *s_bar, *s_dot;
static lv_obj_t *s_icon[UI_TAB_COUNT]; /* el icono de cada boton (lo que se atenua) */
static lv_obj_t *s_btn[UI_TAB_COUNT];  /* el boton: la pestaña activa lleva su pastilla */
#define PILL_W 42
#define PILL_H 32

/* ------------------------------------------------------------- handlers */

static void tab_clicked(lv_event_t *e)
{
    ui_go((ui_tab_t)(intptr_t)lv_event_get_user_data(e)); /* la de ahora no hace nada */
}

static void reset_confirmed(lv_event_t *e)
{
    (void)e;
    demo_mode_factory_reset(); /* sale de la demo, registra `reset`, envia y borra */
}

/* La que antes colgaba de la rueda de Inicio */
static void settings_long_pressed(lv_event_t *e)
{
    (void)e;
    ui_msgbox("IncuTwin", tr(STR_RESET_Q), tr(STR_RESET), COL_CORAL, reset_confirmed, NULL);
}

/* ----------------------------------------------------------------- build */

static lv_obj_t *tab_button(ui_tab_t tab)
{
    /* pastilla de 42x32 centrada en su hueco de 48 bajo la raya de 2 px; la zona de toque, el hueco */
    lv_obj_t *b = lv_button_create(s_bar);
    lv_obj_set_size(b, PILL_W, PILL_H);
    lv_obj_set_pos(b, (int32_t)tab * NAV_BTN_W + (NAV_BTN_W - PILL_W) / 2, 2 + (NAV_H - 2 - PILL_H) / 2 - 1);
    lv_obj_set_ext_click_area(b, 4);
    lv_obj_set_style_bg_color(b, COL_TAB_ON, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(b, COL_TAB_ON, LV_STATE_PRESSED);
    lv_obj_set_style_radius(b, TOY_R_PILL, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    /* SHORT_CLICKED: una pulsacion larga no navega (en Ajustes abre el borrado de fabrica) */
    lv_obj_add_event_cb(b, tab_clicked, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)tab);
    s_btn[tab] = b;
    return b;
}

static lv_obj_t *symbol_label(lv_obj_t *b, const char *sym)
{
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, sym);
    lv_obj_set_style_text_font(l, &lv_font_es_20, 0);
    lv_obj_set_style_text_color(l, COL_NAVY, 0);
    lv_obj_center(l);
    return l;
}

static lv_obj_t *image_icon(lv_obj_t *b, const lv_image_dsc_t *src)
{
    lv_obj_t *i = lv_image_create(b);
    lv_image_set_src(i, src);
    lv_obj_set_clickable(i, false);
    lv_obj_center(i);
    return i;
}

void ui_nav_create(void)
{
    s_bar = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_bar, 240, NAV_H);
    lv_obj_set_pos(s_bar, 0, UI_NAV_Y);
    lv_obj_set_style_bg_color(s_bar, COL_CARD, 0);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_bar, 0, 0);
    lv_obj_set_style_border_width(s_bar, TOY_BORDER, 0); /* raya tinta arriba, como la app */
    lv_obj_set_style_border_side(s_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(s_bar, COL_INK, 0);
    lv_obj_set_style_pad_all(s_bar, 0, 0);
    lv_obj_set_scrollable(s_bar, false);
    lv_obj_set_gesture_bubble(s_bar, false); /* deslizar aqui no cambia de pagina */

    s_icon[UI_TAB_HOME] = symbol_label(tab_button(UI_TAB_HOME), LV_SYMBOL_HOME);

    lv_obj_t *notices = tab_button(UI_TAB_NOTICES);
    s_icon[UI_TAB_NOTICES] = symbol_label(notices, LV_SYMBOL_ENVELOPE);
    /* punto en la esquina superior derecha del sobre (20 px centrado en la pastilla de 42) */
    s_dot = lv_obj_create(notices);
    lv_obj_set_size(s_dot, DOT_D, DOT_D);
    lv_obj_set_pos(s_dot, 30, 2);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_dot, COL_CORAL, 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_dot, 0, 0);
    lv_obj_set_clickable(s_dot, false);
    lv_obj_set_hidden(s_dot, true);

    /* corazon de pixeles de la app: 9x8 celdas de 3 px = 27x24 */
    s_icon[UI_TAB_HEARTS] = toy_heart_img(tab_button(UI_TAB_HEARTS), TOY_HEART_FULL, 3);
    lv_obj_center(s_icon[UI_TAB_HEARTS]);
    s_icon[UI_TAB_HISTORY] = image_icon(tab_button(UI_TAB_HISTORY), &img_chick_28);

    lv_obj_t *set = tab_button(UI_TAB_SETTINGS);
    lv_obj_add_event_cb(set, settings_long_pressed, LV_EVENT_LONG_PRESSED, NULL);
    s_icon[UI_TAB_SETTINGS] = symbol_label(set, LV_SYMBOL_SETTINGS);

    ui_nav_set_active(UI_TAB_HOME);
    lv_obj_set_hidden(s_bar, true); /* hasta que salga Inicio (ui_nav_update) */
}

/* El icono, no el boton, va al 50 %: el punto de avisos sin ver se ve siempre entero */
void ui_nav_set_active(ui_tab_t active)
{
    for (int t = 0; t < UI_TAB_COUNT; t++) {
        lv_opa_t opa = t == (int)active ? LV_OPA_COVER : LV_OPA_50;
        lv_obj_set_style_bg_opa(s_btn[t], t == (int)active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        if (t == UI_TAB_HEARTS || t == UI_TAB_HISTORY) lv_obj_set_style_image_opa(s_icon[t], opa, 0);
        else lv_obj_set_style_text_opa(s_icon[t], opa, 0);
    }
}

void ui_nav_set_visible(bool visible)
{
    if (!s_bar || visible != lv_obj_is_hidden(s_bar)) return;
    lv_obj_set_hidden(s_bar, !visible);
    ESP_LOGI(TAG, "barra de navegacion %s", visible ? "visible" : "oculta");
}

lv_obj_t *ui_nav_obj(void) { return s_bar; }

void ui_nav_set_unread(bool unread)
{
    if (s_dot) lv_obj_set_hidden(s_dot, !unread);
}
