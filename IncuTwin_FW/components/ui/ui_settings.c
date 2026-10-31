/* Pantalla de ajustes (spec settings): idioma, tono informativo, volumen, brillo,
 * pie de informacion. */
#include <stdio.h>

#include "assets.h"
#include "esp_app_desc.h"
#include "identity.h"
#include "settings.h"
#include "sound.h"
#include "ui_internal.h"

static lv_obj_t *s_scr;
static lv_obj_t *s_lbl_back, *s_lbl_title, *s_lbl_lang, *s_lbl_tone, *s_lbl_hint, *s_lbl_sound,
    *s_lbl_bright;
static lv_obj_t *s_btn_es, *s_btn_en;
static lv_obj_t *s_swatches[TWIN_SKIN_COUNT];
static lv_obj_t *s_btn_vol[4];
static lv_obj_t *s_slider;
static lv_obj_t *s_lbl_net, *s_lbl_inc, *s_lbl_ver;
static uint8_t s_skin;

static const ui_str_t VOL_STR[4] = { STR_VOL_OFF, STR_VOL_LOW, STR_VOL_MID, STR_VOL_HIGH };
static const uint32_t SWATCH_COLORS[TWIN_SKIN_COUNT] = { 0xF4C1A6, 0xE9AF8C, 0xD0946C,
                                                         0xAC704E, 0x865438, 0x5C3A28 };

/* ------------------------------------------------------------- handlers */

static void back_clicked(lv_event_t *e) { (void)e; ui_go_home(); }

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

static void swatch_refresh(void)
{
    for (int i = 0; i < TWIN_SKIN_COUNT; i++) {
        bool sel = i == s_skin;
        lv_obj_set_style_border_width(s_swatches[i], sel ? 4 : 1, 0);
        lv_obj_set_style_border_color(s_swatches[i], sel ? COL_NAVY : COL_OFFLINE, 0);
    }
}

/* ----------------------------------------------------------------- build */

lv_obj_t *ui_settings_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_scrollable(s_scr, false);

    lv_obj_t *back = lv_button_create(s_scr);
    lv_obj_set_size(back, 96, 36);
    lv_obj_set_pos(back, 6, 6);
    lv_obj_set_style_bg_color(back, COL_NAVY, 0);
    lv_obj_set_style_radius(back, 12, 0);
    lv_obj_add_event_cb(back, back_clicked, LV_EVENT_CLICKED, NULL);
    s_lbl_back = lv_label_create(back);
    lv_obj_set_style_text_font(s_lbl_back, &lv_font_es_14, 0);
    lv_obj_center(s_lbl_back);

    s_lbl_title = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lbl_title, &lv_font_es_20, 0);
    lv_obj_set_style_text_color(s_lbl_title, COL_NAVY, 0);
    lv_obj_align(s_lbl_title, LV_ALIGN_TOP_RIGHT, -10, 12);

    /* idioma */
    s_lbl_lang = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lbl_lang, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_lbl_lang, COL_NAVY_DARK, 0);
    lv_obj_set_pos(s_lbl_lang, 12, 50);
    s_btn_es = toggle_button(s_scr, 12, 68, 104, 34, "Español", &lv_font_es_16, lang_clicked, 0);
    s_btn_en = toggle_button(s_scr, 124, 68, 104, 34, "English", &lv_font_es_16, lang_clicked, 1);

    /* tono de piel (informativo) */
    s_lbl_tone = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lbl_tone, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_lbl_tone, COL_NAVY_DARK, 0);
    lv_obj_set_pos(s_lbl_tone, 12, 110);
    for (int i = 0; i < TWIN_SKIN_COUNT; i++) {
        s_swatches[i] = lv_obj_create(s_scr);
        lv_obj_set_size(s_swatches[i], 30, 30);
        lv_obj_set_pos(s_swatches[i], 12 + i * 37, 128);
        lv_obj_set_style_radius(s_swatches[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_swatches[i], lv_color_hex(SWATCH_COLORS[i]), 0);
        lv_obj_set_style_shadow_width(s_swatches[i], 0, 0);
        lv_obj_set_clickable(s_swatches[i], false);
        lv_obj_set_scrollable(s_swatches[i], false);
    }
    s_lbl_hint = small_label(s_scr, 12, 162, COL_OFFLINE);
    lv_obj_set_size(s_lbl_hint, 216, 28); /* dos lineas */
    lv_label_set_long_mode(s_lbl_hint, LV_LABEL_LONG_MODE_WRAP);

    /* volumen */
    s_lbl_sound = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lbl_sound, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_lbl_sound, COL_NAVY_DARK, 0);
    lv_obj_set_pos(s_lbl_sound, 12, 192);
    static const int32_t VOL_X[4] = { 12, 74, 124, 182 };
    static const int32_t VOL_W[4] = { 56, 44, 52, 46 };
    for (int i = 0; i < 4; i++) {
        s_btn_vol[i] = toggle_button(s_scr, VOL_X[i], 210, VOL_W[i], 30, "", &lv_font_es_12,
                                     vol_clicked, i);
    }

    /* brillo */
    s_lbl_bright = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lbl_bright, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_lbl_bright, COL_NAVY_DARK, 0);
    lv_obj_set_pos(s_lbl_bright, 12, 248);
    s_slider = lv_slider_create(s_scr);
    lv_obj_set_size(s_slider, 130, 10);
    lv_obj_set_pos(s_slider, 96, 253);
    lv_slider_set_range(s_slider, SETTINGS_BRIGHT_MIN, SETTINGS_BRIGHT_MAX);
    lv_obj_set_style_bg_color(s_slider, COL_NAVY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, COL_NAVY, LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider, slider_changed, LV_EVENT_VALUE_CHANGED, NULL);

    /* pie */
    s_lbl_net = small_label(s_scr, 12, 276, COL_NAVY_DARK);
    s_lbl_inc = small_label(s_scr, 12, 290, COL_NAVY_DARK);
    s_lbl_ver = small_label(s_scr, 12, 304, COL_OFFLINE);
    lv_label_set_text_fmt(s_lbl_ver, "%s · v%s", identity_sn(), esp_app_get_description()->version);

    ui_settings_retranslate();
    app_evt_settings_t st = { .lang = settings_lang(), .volume = settings_volume(),
                              .brightness = settings_brightness() };
    ui_settings_update(&st);
    return s_scr;
}

void ui_settings_retranslate(void)
{
    lv_label_set_text_fmt(s_lbl_back, LV_SYMBOL_LEFT " %s", tr(STR_BACK));
    lv_label_set_text(s_lbl_title, tr(STR_SETTINGS));
    lv_label_set_text(s_lbl_lang, tr(STR_LANGUAGE));
    lv_label_set_text(s_lbl_tone, tr(STR_SKIN_TONE));
    lv_label_set_text(s_lbl_hint, tr(STR_SKIN_HINT));
    lv_label_set_text(s_lbl_sound, tr(STR_SOUND));
    lv_label_set_text(s_lbl_bright, tr(STR_BRIGHTNESS));
    for (int i = 0; i < 4; i++) lv_label_set_text(lv_obj_get_child(s_btn_vol[i], 0), tr(VOL_STR[i]));
}

void ui_settings_update(const app_evt_settings_t *st)
{
    toggle_set(s_btn_es, st->lang == SETTINGS_LANG_ES);
    toggle_set(s_btn_en, st->lang == SETTINGS_LANG_EN);
    for (int i = 0; i < 4; i++) toggle_set(s_btn_vol[i], st->volume == i);
    if (!lv_obj_has_state(s_slider, LV_STATE_PRESSED)) {
        lv_slider_set_value(s_slider, st->brightness, LV_ANIM_OFF);
    }
}

void ui_settings_apply(const twin_snapshot_t *s)
{
    s_skin = s->inc.skin;
    swatch_refresh();
    if (s->link.wifi) {
        lv_label_set_text_fmt(s_lbl_net, "%s: %s · %s: %s", tr(STR_WIFI), s->link.ip, tr(STR_SERVER),
                              tr(s->link.broker_connected ? STR_CONNECTED : STR_NO_CONNECTION));
    } else {
        lv_label_set_text_fmt(s_lbl_net, "%s: %s", tr(STR_WIFI), tr(STR_NO_CONNECTION));
    }
    lv_label_set_text_fmt(s_lbl_inc, "%s: %s", tr(STR_INCUNEST),
                          s->link.paired ? s->link.incubator_id : "—");
}
