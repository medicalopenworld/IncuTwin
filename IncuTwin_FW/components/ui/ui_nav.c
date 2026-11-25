/* Barra fija de navegacion (spec hmi-navegacion-avisos): 240x40 en y = 280, abajo de Inicio,
 * Historial y Ajustes. Tres botones de 80x40: casa, pollito (con un punto coral si hay avisos sin
 * ver) y rueda dentada. El de la pantalla en la que se esta, en azul de la marca y opaco; los
 * otros, al 50 %. Pulsacion larga (1 s, la del indev) en Ajustes: borrado de fabrica.
 * Va en cada pantalla, no en la capa superior, para no salir en splash, onboarding ni dialogos. */
#include "assets.h"
#include "storage.h"
#include "ui_internal.h"

#define NAV_H     40
#define NAV_BTN_W 80
#define DOT_D     8
#define N_TABS    3

static lv_obj_t *s_dots[N_TABS]; /* punto de cada barra (una por pantalla) */

/* ------------------------------------------------------------- handlers */

static void tab_clicked(lv_event_t *e)
{
    ui_go((ui_tab_t)(intptr_t)lv_event_get_user_data(e));
}

static void reset_confirmed(lv_event_t *e)
{
    (void)e;
    storage_factory_reset();
}

/* La que antes colgaba de la rueda de Inicio */
static void settings_long_pressed(lv_event_t *e)
{
    (void)e;
    ui_msgbox("IncuTwin", tr(STR_RESET_Q), tr(STR_RESET), COL_RED, reset_confirmed, NULL);
}

/* ----------------------------------------------------------------- build */

static lv_obj_t *tab_button(lv_obj_t *bar, ui_tab_t tab, ui_tab_t active)
{
    lv_obj_t *b = lv_button_create(bar);
    lv_obj_set_size(b, NAV_BTN_W, NAV_H - 1); /* debajo del filete de 1 px de arriba */
    lv_obj_set_pos(b, (int32_t)tab * NAV_BTN_W, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(b, COL_BG_PHOTO, LV_STATE_PRESSED);
    lv_obj_set_style_radius(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    /* SHORT_CLICKED: una pulsacion larga no navega (en Ajustes abre el borrado de fabrica) */
    if (tab != active) lv_obj_add_event_cb(b, tab_clicked, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)tab);
    return b;
}

/* El icono, no el boton, va al 50 %: el punto de avisos sin ver se ve siempre entero */
static lv_opa_t icon_opa(ui_tab_t tab, ui_tab_t active) { return tab == active ? LV_OPA_COVER : LV_OPA_50; }

static void symbol_label(lv_obj_t *b, const char *sym, lv_opa_t opa)
{
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, sym);
    lv_obj_set_style_text_font(l, &lv_font_es_20, 0);
    lv_obj_set_style_text_color(l, COL_NAVY, 0);
    lv_obj_set_style_text_opa(l, opa, 0);
    lv_obj_center(l);
}

lv_obj_t *ui_nav_create(lv_obj_t *scr, ui_tab_t active)
{
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, 240, NAV_H);
    lv_obj_set_pos(bar, 0, UI_NAV_Y);
    lv_obj_set_style_bg_color(bar, COL_CARD, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(bar, COL_OFFLINE, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_scrollable(bar, false);
    lv_obj_set_gesture_bubble(bar, false); /* deslizar aqui no cambia de pagina */

    symbol_label(tab_button(bar, UI_TAB_HOME, active), LV_SYMBOL_HOME, icon_opa(UI_TAB_HOME, active));

    lv_obj_t *hist = tab_button(bar, UI_TAB_HISTORY, active);
    lv_obj_t *chick = lv_image_create(hist);
    lv_image_set_src(chick, &img_chick_28);
    lv_obj_set_clickable(chick, false);
    lv_obj_set_style_image_opa(chick, icon_opa(UI_TAB_HISTORY, active), 0);
    lv_obj_center(chick);
    /* punto en la esquina superior derecha del pollito (28 px centrado: x 26..54, y 6..34) */
    lv_obj_t *dot = lv_obj_create(hist);
    lv_obj_set_size(dot, DOT_D, DOT_D);
    lv_obj_set_pos(dot, 50, 3);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, COL_CORAL, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_clickable(dot, false);
    lv_obj_set_hidden(dot, true);
    s_dots[active] = dot;

    lv_obj_t *set = tab_button(bar, UI_TAB_SETTINGS, active);
    lv_obj_add_event_cb(set, settings_long_pressed, LV_EVENT_LONG_PRESSED, NULL);
    symbol_label(set, LV_SYMBOL_SETTINGS, icon_opa(UI_TAB_SETTINGS, active));
    return bar;
}

void ui_nav_set_unread(bool unread)
{
    for (int i = 0; i < N_TABS; i++) {
        if (s_dots[i]) lv_obj_set_hidden(s_dots[i], !unread);
    }
}
