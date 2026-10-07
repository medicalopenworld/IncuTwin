/* Pantalla principal (spec twin-display). Layout vertical 240x320:
 *   barra superior : WiFi | marca | engranaje
 *   centro         : bebe + halo | incubadora vacia | con sus papas
 *   fila           : 3 iconos de estado + boton "Agarra mi mano"
 *   abajo          : barra de estado siempre visible */
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "baby_widget.h"
#include "sound.h"
#include "storage.h"
#include "twin_logic.h"
#include "ui_internal.h"
#include "ui_onboarding.h"
#include "usage_stats.h"

#define BABY_X 63   /* sprite de 114x185 */
#define BABY_Y 38
#define BABY_CX (BABY_X + 57)
#define BABY_CY (BABY_Y + 92)
#define ICON_D 44
#define ICON_Y 232
#define STATUS_Y 288

typedef enum { HALO_OFF = 0, HALO_CALM, HALO_WARM, HALO_PHOTO, HALO_ALARM } halo_mode_t;
typedef enum { VIEW_EMPTY = 0, VIEW_BABY, VIEW_PARENTS } home_view_t;

static lv_obj_t *s_scr;
static lv_obj_t *s_halo, *s_empty_img, *s_parents_img, *s_wifi_img, *s_demo_badge;
static lv_obj_t *s_icon_thermo, *s_icon_photo, *s_icon_heart;
static lv_obj_t *s_btn_hand, *s_lbl_hand;
static lv_obj_t *s_status, *s_lbl_status;
static halo_mode_t s_halo_mode = HALO_CALM;
static bool s_awake_real; /* para restaurar al soltar el dedo */

/* ------------------------------------------------------------------ halo */

static void halo_opa_cb(void *var, int32_t v) { lv_obj_set_style_bg_opa((lv_obj_t *)var, (lv_opa_t)v, 0); }

static void halo_start_anim(uint32_t period)
{
    lv_anim_delete(s_halo, halo_opa_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_halo);
    lv_anim_set_exec_cb(&a, halo_opa_cb);
    lv_anim_set_values(&a, LV_OPA_20, LV_OPA_60);
    lv_anim_set_duration(&a, period);
    lv_anim_set_reverse_duration(&a, period);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void halo_set_mode(halo_mode_t m)
{
    if (m == s_halo_mode) return;
    s_halo_mode = m;
    lv_color_t col = COL_HALO_CALM;
    uint32_t period = 2600;
    switch (m) {
    case HALO_OFF:
        lv_obj_set_hidden(s_halo, true);
        lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
        return;
    case HALO_WARM:  col = COL_HALO_WARM; break;
    case HALO_PHOTO: col = COL_HALO_PHOTO; period = 2000; break;
    case HALO_ALARM: col = COL_HALO_ALARM; period = 700; break;
    default: break;
    }
    lv_obj_set_hidden(s_halo, false);
    lv_obj_set_style_bg_color(s_halo, col, 0);
    lv_obj_set_style_bg_color(s_scr, m == HALO_PHOTO ? COL_BG_PHOTO : COL_BG, 0);
    halo_start_anim(period);
}

/* ----------------------------------------------------------- state icons */

static lv_obj_t *state_icon_create(lv_obj_t *parent, int32_t x, const void *src)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, ICON_D, ICON_D);
    lv_obj_set_pos(c, x, ICON_Y);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c, COL_CARD, 0);
    lv_obj_set_style_border_width(c, 3, 0);
    lv_obj_set_style_border_color(c, COL_OFFLINE, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_set_scrollable(c, false);
    lv_obj_set_clickable(c, false);
    lv_obj_t *icon = lv_image_create(c);
    lv_image_set_src(icon, src);
    lv_obj_center(icon);
    return c;
}

static void state_icon_set(lv_obj_t *c, lv_color_t col) { lv_obj_set_style_border_color(c, col, 0); }

/* ------------------------------------------------------------- handlers */

static void gear_clicked(lv_event_t *e)
{
    (void)e;
    ui_go_settings();
}

static void reset_confirmed(lv_event_t *e)
{
    (void)e;
    storage_factory_reset();
}

static void gear_long_pressed(lv_event_t *e)
{
    (void)e;
    ui_msgbox("IncuTwin", tr(STR_RESET_Q), tr(STR_RESET), COL_RED, reset_confirmed, NULL);
}

/* Icono WiFi -> cambiar de red (spec onboarding) */
static void wifi_change_confirmed(lv_event_t *e)
{
    lv_obj_t *mb = lv_event_get_user_data(e);
    lv_msgbox_close(mb);
    ui_onboarding_start_wifi_only();
}

static void wifi_icon_clicked(lv_event_t *e)
{
    (void)e;
    ui_msgbox("WiFi", tr(STR_WIFI_CHANGE_Q), tr(STR_WIFI_CHANGE), COL_NAVY, wifi_change_confirmed, NULL);
}

/* "Agarra mi mano": tocar al bebe o el boton lo despierta, suena el latido y cuenta uso */
static void baby_touched(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_PRESSED) {
        usage_hand_set(true);
        sound_hand_hold(true);
        baby_set_awake(true);
    } else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) {
        usage_hand_set(false);
        sound_hand_hold(false);
        baby_set_awake(s_awake_real);
    }
}

/* ----------------------------------------------------------------- build */

lv_obj_t *ui_home_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_scrollable(s_scr, false);

    /* imagenes a pantalla completa (debajo de todo) */
    s_empty_img = lv_image_create(s_scr);
    lv_image_set_src(s_empty_img, &img_incunest_empty);
    lv_obj_set_pos(s_empty_img, 0, 0);
    lv_obj_set_hidden(s_empty_img, true);

    s_parents_img = lv_image_create(s_scr);
    lv_image_set_src(s_parents_img, &img_baby_parents);
    lv_obj_set_pos(s_parents_img, 0, 0);
    lv_obj_set_hidden(s_parents_img, true);

    /* halo que respira */
    s_halo = lv_obj_create(s_scr);
    lv_obj_set_size(s_halo, 190, 190);
    lv_obj_set_style_radius(s_halo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_halo, COL_HALO_CALM, 0);
    lv_obj_set_style_bg_opa(s_halo, LV_OPA_30, 0);
    lv_obj_set_style_border_width(s_halo, 0, 0);
    lv_obj_set_pos(s_halo, BABY_CX - 95, BABY_CY - 95);
    lv_obj_set_scrollable(s_halo, false);
    lv_obj_set_clickable(s_halo, false);
    halo_start_anim(2600);

    lv_obj_t *baby = baby_widget_create(s_scr, BABY_X, BABY_Y);
    lv_obj_set_clickable(baby, true);
    lv_obj_add_event_cb(baby, baby_touched, LV_EVENT_ALL, NULL);

    s_icon_thermo = state_icon_create(s_scr, 8, &img_icon_thermo);
    s_icon_photo = state_icon_create(s_scr, 58, &img_icon_lamp);
    s_icon_heart = state_icon_create(s_scr, 108, &img_heart);

    s_btn_hand = lv_button_create(s_scr);
    lv_obj_set_size(s_btn_hand, 74, ICON_D);
    lv_obj_set_pos(s_btn_hand, 158, ICON_Y);
    lv_obj_set_style_bg_color(s_btn_hand, COL_CORAL, 0);
    lv_obj_set_style_radius(s_btn_hand, 14, 0);
    lv_obj_set_style_shadow_width(s_btn_hand, 0, 0);
    lv_obj_add_event_cb(s_btn_hand, baby_touched, LV_EVENT_ALL, NULL);
    s_lbl_hand = lv_label_create(s_btn_hand);
    lv_label_set_text(s_lbl_hand, tr(STR_HAND));
    lv_obj_set_style_text_font(s_lbl_hand, &lv_font_es_12, 0);
    lv_obj_set_style_text_align(s_lbl_hand, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_lbl_hand, 66);
    lv_label_set_long_mode(s_lbl_hand, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_center(s_lbl_hand);

    /* barra de estado */
    s_status = lv_obj_create(s_scr);
    lv_obj_set_size(s_status, 224, 26);
    lv_obj_set_pos(s_status, 8, STATUS_Y);
    lv_obj_set_style_radius(s_status, 10, 0);
    lv_obj_set_style_bg_color(s_status, COL_CARD, 0);
    lv_obj_set_style_border_width(s_status, 2, 0);
    lv_obj_set_style_border_color(s_status, COL_OFFLINE, 0);
    lv_obj_set_style_pad_all(s_status, 0, 0);
    lv_obj_set_scrollable(s_status, false);
    lv_obj_set_clickable(s_status, false);
    s_lbl_status = lv_label_create(s_status);
    lv_label_set_text(s_lbl_status, "");
    lv_obj_set_style_text_font(s_lbl_status, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_lbl_status, COL_OFFLINE, 0);
    lv_obj_set_size(s_lbl_status, 216, 18); /* alto fijo: DOTS recorta en vez de saltar de linea */
    lv_label_set_long_mode(s_lbl_status, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(s_lbl_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_lbl_status);

    /* barra superior */
    s_wifi_img = lv_image_create(s_scr);
    lv_image_set_src(s_wifi_img, &img_wifi_off);
    lv_obj_set_pos(s_wifi_img, 8, 8);
    lv_obj_set_clickable(s_wifi_img, true); /* tocar: cambiar de red WiFi */
    lv_obj_set_ext_click_area(s_wifi_img, 10);
    lv_obj_add_event_cb(s_wifi_img, wifi_icon_clicked, LV_EVENT_SHORT_CLICKED, NULL);

    s_demo_badge = lv_label_create(s_scr);
    lv_label_set_text(s_demo_badge, "DEMO");
    lv_obj_set_style_text_font(s_demo_badge, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(s_demo_badge, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_demo_badge, COL_CORAL, 0);
    lv_obj_set_style_bg_opa(s_demo_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_demo_badge, 8, 0);
    lv_obj_set_style_pad_hor(s_demo_badge, 4, 0);
    lv_obj_set_style_pad_ver(s_demo_badge, 3, 0);
    lv_obj_set_pos(s_demo_badge, 6, 11);
    lv_obj_set_hidden(s_demo_badge, true);

    lv_obj_t *wm = lv_image_create(s_scr);
    lv_image_set_src(wm, &img_wordmark);
    lv_obj_align(wm, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *gear = lv_button_create(s_scr);
    lv_obj_set_size(gear, 42, 30);
    lv_obj_align(gear, LV_ALIGN_TOP_RIGHT, -4, 5);
    lv_obj_set_style_bg_color(gear, COL_CARD, 0);
    lv_obj_set_style_shadow_width(gear, 0, 0);
    lv_obj_set_style_radius(gear, 10, 0);
    lv_obj_add_event_cb(gear, gear_clicked, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(gear, gear_long_pressed, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_t *gl = lv_label_create(gear);
    lv_label_set_text(gl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(gl, &lv_font_es_16, 0);
    lv_obj_set_style_text_color(gl, COL_NAVY, 0);
    lv_obj_center(gl);
    return s_scr;
}

void ui_home_retranslate(void) { lv_label_set_text(s_lbl_hand, tr(STR_HAND)); }

/* ------------------------------------------------------------ state -> UI */

static void status_bar_text(const char *txt, lv_color_t col)
{
    if (strcmp(lv_label_get_text(s_lbl_status), txt) != 0) lv_label_set_text(s_lbl_status, txt);
    lv_obj_set_style_text_color(s_lbl_status, col, 0);
    lv_obj_set_style_border_color(s_status, col, 0);
}

/* "Lucía · 1250 g · 3 días" (solo los campos compartidos) */
static void status_bar_baby(const twin_incubator_t *inc)
{
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "%s", inc->name);
    if (inc->weight_g > 0 && n < (int)sizeof(buf))
        n += snprintf(buf + n, sizeof(buf) - n, " · %u g", inc->weight_g);
    if (inc->age_d >= 0 && n < (int)sizeof(buf))
        snprintf(buf + n, sizeof(buf) - n, " · %d %s", inc->age_d,
                 tr(inc->age_d == 1 ? STR_DAY : STR_DAYS));
    status_bar_text(buf, COL_OK);
}

static void status_bar_apply(const twin_snapshot_t *s)
{
    switch (twin_status_of(s)) {
    case TWIN_ST_HOME:        status_bar_text(tr(STR_ST_HOME), COL_OK); break;
    case TWIN_ST_PARENTS:     status_bar_text(tr(STR_ST_PARENTS), COL_OK); break;
    case TWIN_ST_NO_WIFI:     status_bar_text(tr(STR_ST_NO_WIFI), COL_RED); break;
    case TWIN_ST_NO_CREDS:    status_bar_text(tr(STR_ST_NO_CREDS), COL_RED); break;
    case TWIN_ST_CONNECTING:  status_bar_text(tr(STR_ST_CONNECTING), COL_WARM); break;
    case TWIN_ST_BROKER_LOST: status_bar_text(tr(STR_ST_BROKER_LOST), COL_RED); break;
    case TWIN_ST_UNPAIRED:    status_bar_text(tr(STR_ST_UNPAIRED), COL_OFFLINE); break;
    case TWIN_ST_WAITING:     status_bar_text(tr(STR_ST_WAITING), COL_OFFLINE); break;
    case TWIN_ST_INC_OFF:     status_bar_text(tr(STR_ST_INC_OFF), COL_OFFLINE); break;
    case TWIN_ST_NO_BABY:     status_bar_text(tr(STR_ST_NO_BABY), COL_PHOTO); break;
    case TWIN_ST_ALARM:       status_bar_text(tr(STR_ST_ALARM), COL_RED); break;
    case TWIN_ST_NAME:        status_bar_baby(&s->inc); break;
    case TWIN_ST_AWAKE:       status_bar_text(tr(STR_ST_AWAKE), COL_OK); break;
    default:                  status_bar_text(tr(STR_ST_SLEEP), COL_OK); break;
    }
}

static void home_set_view(home_view_t view)
{
    lv_obj_t *widgets[] = { baby_widget_obj(), s_icon_thermo, s_icon_photo, s_icon_heart, s_btn_hand };
    for (size_t i = 0; i < sizeof(widgets) / sizeof(widgets[0]); i++) {
        lv_obj_set_hidden(widgets[i], view != VIEW_BABY);
    }
    lv_obj_set_hidden(s_empty_img, view != VIEW_EMPTY);
    lv_obj_set_hidden(s_parents_img, view != VIEW_PARENTS);
}

static void wifi_icon_apply(const twin_snapshot_t *s)
{
    if (s->demo) {
        lv_obj_set_hidden(s_wifi_img, true);
        lv_obj_set_hidden(s_demo_badge, false);
        return;
    }
    lv_obj_set_hidden(s_demo_badge, true);
    lv_obj_set_hidden(s_wifi_img, false);
    if (!s->link.wifi) {
        lv_image_set_src(s_wifi_img, &img_wifi_off);
        lv_obj_set_style_image_recolor_opa(s_wifi_img, LV_OPA_TRANSP, 0);
        return;
    }
    static const lv_image_dsc_t *const LVL[] = { &img_wifi_0, &img_wifi_1, &img_wifi_2, &img_wifi_3 };
    lv_image_set_src(s_wifi_img, LVL[twin_rssi_level(s->link.rssi)]);
    if (s->link.broker_connected) {
        lv_obj_set_style_image_recolor_opa(s_wifi_img, LV_OPA_TRANSP, 0);
    } else { /* ambar: hay WiFi pero no sesion con el broker */
        lv_obj_set_style_image_recolor(s_wifi_img, COL_WARM, 0);
        lv_obj_set_style_image_recolor_opa(s_wifi_img, LV_OPA_COVER, 0);
    }
}

void ui_home_apply(const twin_snapshot_t *s)
{
    const twin_incubator_t *inc = &s->inc;
    s_awake_real = inc->awake;

    wifi_icon_apply(s);
    home_set_view(s->parents_view ? VIEW_PARENTS : s->show_baby ? VIEW_BABY : VIEW_EMPTY);
    status_bar_apply(s);

    if (!s->show_baby) {
        state_icon_set(s_icon_thermo, COL_OFFLINE);
    } else {
        switch (inc->thermo) {
        case TWIN_THERMO_HEATING: state_icon_set(s_icon_thermo, COL_WARM); break;
        case TWIN_THERMO_STABLE:  state_icon_set(s_icon_thermo, COL_OK); break;
        case TWIN_THERMO_ALARM:   state_icon_set(s_icon_thermo, COL_RED); break;
        default:                  state_icon_set(s_icon_thermo, COL_OFFLINE); break;
        }
    }
    state_icon_set(s_icon_photo, s->show_baby && inc->photo ? COL_PHOTO : COL_OFFLINE);
    state_icon_set(s_icon_heart, s->show_baby && inc->bpm > 0 ? COL_CORAL : COL_OFFLINE);

    baby_set_skin_tone(inc->skin);
    baby_set_awake(inc->awake);
    baby_set_heart_rate(s->show_baby ? inc->bpm : 0);

    if (!s->show_baby)                          halo_set_mode(HALO_OFF);
    else if (inc->thermo == TWIN_THERMO_ALARM)   halo_set_mode(HALO_ALARM);
    else if (inc->photo)                         halo_set_mode(HALO_PHOTO);
    else if (inc->thermo == TWIN_THERMO_HEATING) halo_set_mode(HALO_WARM);
    else                                         halo_set_mode(HALO_CALM);
}
