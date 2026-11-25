/* Pantalla de ajustes (spec settings, hmi-navegacion-avisos): idioma, volumen, brillo, horario
 * nocturno ("De noche, sin sonido": interruptor y horas desde/hasta) y pie de informacion (red y
 * version). Sin tono de piel ni numero de IncuNest. Sin boton "atras": la barra de navegacion de
 * abajo (y 280..320) lo sustituye; todo lo demas acaba por encima de ella. */
#include <stdio.h>

#include "assets.h"
#include "esp_app_desc.h"
#include "identity.h"
#include "settings.h"
#include "sound.h"
#include "twin_model.h"
#include "ui_internal.h"

static lv_obj_t *s_scr;
static lv_obj_t *s_lbl_title, *s_lbl_lang, *s_lbl_sound, *s_lbl_bright;
static lv_obj_t *s_btn_es, *s_btn_en;
static lv_obj_t *s_btn_vol[4];
static lv_obj_t *s_slider;
static lv_obj_t *s_lbl_net, *s_lbl_ver;
static lv_obj_t *s_lbl_quiet, *s_sw_quiet;
/* horario nocturno: [0] desde, [1] hasta */
static lv_obj_t *s_lbl_qcap[2], *s_lbl_qval[2], *s_btn_qminus[2], *s_btn_qplus[2];

static const ui_str_t VOL_STR[4] = { STR_VOL_OFF, STR_VOL_LOW, STR_VOL_MID, STR_VOL_HIGH };

/* ------------------------------------------------------------- handlers */

static void lang_clicked(lv_event_t *e)
{
    settings_set_lang((uint8_t)(intptr_t)lv_event_get_user_data(e));
}

static void vol_clicked(lv_event_t *e)
{
    uint8_t level = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (level == settings_volume()) return;
    settings_set_volume(level);
    if (level > 0) sound_request(SOUND_TEST); /* oir el nuevo nivel */
}

static void slider_changed(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target(e);
    settings_set_brightness((uint8_t)lv_slider_get_value(sl));
}

static void quiet_switched(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    settings_set_quiet_on(lv_obj_has_state(sw, LV_STATE_CHECKED) ? 1 : 0);
}

/* user_data: bit 0 = cual (0 desde, 1 hasta), bit 1 = sube (+1 h) o baja (-1 h); da la vuelta */
static void quiet_step(lv_event_t *e)
{
    int ud = (int)(intptr_t)lv_event_get_user_data(e);
    bool to = ud & 1;
    int step = (ud & 2) ? 1 : 23;
    if (to) {
        settings_set_quiet_to((uint8_t)((settings_quiet_to() + step) % 24));
    } else {
        settings_set_quiet_from((uint8_t)((settings_quiet_from() + step) % 24));
    }
}

/* ----------------------------------------------------------- helpers */

static lv_obj_t *toggle_button(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                               const char *txt, const lv_font_t *font, lv_event_cb_t cb, int ud)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, COL_NAVY, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(intptr_t)ud);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_center(l);
    return b;
}

static void toggle_set(lv_obj_t *b, bool sel)
{
    lv_obj_set_style_bg_color(b, sel ? COL_NAVY : COL_CARD, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(b, 0), sel ? lv_color_white() : COL_NAVY, 0);
}

static lv_obj_t *section_label(lv_obj_t *parent, int32_t x, int32_t y)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(l, COL_NAVY_DARK, 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

static lv_obj_t *small_label(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t col)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_set_pos(l, x, y);
    /* alto fijo de una linea: en LVGL 9 el modo DOTS solo recorta si el tamano es fijo;
     * con alto automatico el texto largo salta a una segunda linea y se solapa */
    lv_obj_set_size(l, 216, 14);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    return l;
}

/* ----------------------------------------------------------------- build */

lv_obj_t *ui_settings_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_scrollable(s_scr, false);

    /* titulo arriba, centrado (como el del Historial) */
    s_lbl_title = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lbl_title, &lv_font_es_20, 0);
    lv_obj_set_style_text_color(s_lbl_title, COL_NAVY, 0);
    lv_obj_align(s_lbl_title, LV_ALIGN_TOP_MID, 0, 9);

    /* idioma */
    s_lbl_lang = section_label(s_scr, 12, 38);
    s_btn_es = toggle_button(s_scr, 12, 56, 104, 30, "Español", &lv_font_es_16, lang_clicked, 0);
    s_btn_en = toggle_button(s_scr, 124, 56, 104, 30, "English", &lv_font_es_16, lang_clicked, 1);

    /* volumen */
    s_lbl_sound = section_label(s_scr, 12, 92);
    static const int32_t VOL_X[4] = { 12, 74, 124, 182 };
    static const int32_t VOL_W[4] = { 56, 44, 52, 46 };
    for (int i = 0; i < 4; i++) {
        s_btn_vol[i] = toggle_button(s_scr, VOL_X[i], 110, VOL_W[i], 30, "", &lv_font_es_12,
                                     vol_clicked, i);
    }

    /* brillo */
    s_lbl_bright = section_label(s_scr, 12, 146);
    s_slider = lv_slider_create(s_scr);
    lv_obj_set_size(s_slider, 130, 10);
    lv_obj_set_pos(s_slider, 96, 151);
    lv_slider_set_range(s_slider, SETTINGS_BRIGHT_MIN, SETTINGS_BRIGHT_MAX);
    lv_obj_set_style_bg_color(s_slider, COL_NAVY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, COL_NAVY, LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider, slider_changed, LV_EVENT_VALUE_CHANGED, NULL);

    /* de noche, sin sonido: interruptor y, debajo, desde / hasta en horas enteras */
    s_lbl_quiet = section_label(s_scr, 12, 172);
    lv_obj_set_width(s_lbl_quiet, 164);
    lv_label_set_long_mode(s_lbl_quiet, LV_LABEL_LONG_MODE_DOTS);
    s_sw_quiet = lv_switch_create(s_scr);
    lv_obj_set_size(s_sw_quiet, 44, 24);
    lv_obj_set_pos(s_sw_quiet, 184, 168);
    lv_obj_set_style_bg_color(s_sw_quiet, COL_NAVY, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_ext_click_area(s_sw_quiet, 6);
    lv_obj_add_event_cb(s_sw_quiet, quiet_switched, LV_EVENT_VALUE_CHANGED, NULL);
    for (int i = 0; i < 2; i++) {
        int32_t x = 12 + i * 112;
        s_lbl_qcap[i] = small_label(s_scr, x, 196, COL_NAVY_DARK);
        lv_obj_set_width(s_lbl_qcap[i], 104);
        s_btn_qminus[i] = toggle_button(s_scr, x, 212, 30, 28, LV_SYMBOL_MINUS, &lv_font_es_14, quiet_step, i);
        s_btn_qplus[i] = toggle_button(s_scr, x + 74, 212, 30, 28, LV_SYMBOL_PLUS, &lv_font_es_14, quiet_step,
                                       i | 2);
        toggle_set(s_btn_qminus[i], false);
        toggle_set(s_btn_qplus[i], false);
        s_lbl_qval[i] = lv_label_create(s_scr);
        lv_obj_set_style_text_font(s_lbl_qval[i], &lv_font_es_16, 0);
        lv_obj_set_style_text_color(s_lbl_qval[i], COL_NAVY, 0);
        lv_obj_set_style_text_align(s_lbl_qval[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_size(s_lbl_qval[i], 44, 18);
        lv_obj_set_pos(s_lbl_qval[i], x + 30, 217);
    }

    /* pie, encima de la barra de navegacion */
    s_lbl_net = small_label(s_scr, 12, 246, COL_NAVY_DARK);
    s_lbl_ver = small_label(s_scr, 12, 262, COL_OFFLINE);
    lv_label_set_text_fmt(s_lbl_ver, "%s · v%s", identity_sn(), esp_app_get_description()->version);

    ui_settings_retranslate();
    app_evt_settings_t st = { .lang = settings_lang(), .volume = settings_volume(),
                              .brightness = settings_brightness(),
                              .quiet_on = settings_quiet_on(), .quiet_from = settings_quiet_from(),
                              .quiet_to = settings_quiet_to() };
    ui_settings_update(&st);
    ui_nav_create(s_scr, UI_TAB_SETTINGS);
    return s_scr;
}

void ui_settings_retranslate(void)
{
    lv_label_set_text(s_lbl_title, tr(STR_SETTINGS));
    lv_label_set_text(s_lbl_lang, tr(STR_LANGUAGE));
    lv_label_set_text(s_lbl_sound, tr(STR_SOUND));
    lv_label_set_text(s_lbl_bright, tr(STR_BRIGHTNESS));
    for (int i = 0; i < 4; i++) lv_label_set_text(lv_obj_get_child(s_btn_vol[i], 0), tr(VOL_STR[i]));
    lv_label_set_text(s_lbl_quiet, tr(STR_QUIET));
    lv_label_set_text(s_lbl_qcap[0], tr(STR_QUIET_FROM));
    lv_label_set_text(s_lbl_qcap[1], tr(STR_QUIET_TO));
}

void ui_settings_update(const app_evt_settings_t *st)
{
    toggle_set(s_btn_es, st->lang == SETTINGS_LANG_ES);
    toggle_set(s_btn_en, st->lang == SETTINGS_LANG_EN);
    for (int i = 0; i < 4; i++) toggle_set(s_btn_vol[i], st->volume == i);
    if (!lv_obj_has_state(s_slider, LV_STATE_PRESSED)) {
        lv_slider_set_value(s_slider, st->brightness, LV_ANIM_OFF);
    }
    lv_obj_set_state(s_sw_quiet, LV_STATE_CHECKED, st->quiet_on != 0);
    const uint8_t hour[2] = { st->quiet_from, st->quiet_to };
    for (int i = 0; i < 2; i++) {
        lv_label_set_text_fmt(s_lbl_qval[i], "%u:00", (unsigned)hour[i]);
        /* apagado: las horas se ven, atenuadas y sin poder tocarlas */
        lv_obj_set_state(s_btn_qminus[i], LV_STATE_DISABLED, !st->quiet_on);
        lv_obj_set_state(s_btn_qplus[i], LV_STATE_DISABLED, !st->quiet_on);
        lv_opa_t opa = st->quiet_on ? LV_OPA_COVER : LV_OPA_50;
        lv_obj_set_style_opa(s_btn_qminus[i], opa, 0);
        lv_obj_set_style_opa(s_btn_qplus[i], opa, 0);
        lv_obj_set_style_text_opa(s_lbl_qval[i], opa, 0);
    }
}

void ui_settings_apply(const twin_snapshot_t *s)
{
    if (s->link.wifi) {
        lv_label_set_text_fmt(s_lbl_net, "%s: %s · %s: %s", tr(STR_WIFI), s->link.ip, tr(STR_SERVER),
                              tr(s->link.broker_connected ? STR_CONNECTED : STR_NO_CONNECTION));
    } else {
        lv_label_set_text_fmt(s_lbl_net, "%s: %s", tr(STR_WIFI), tr(STR_NO_CONNECTION));
    }
}
