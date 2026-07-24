#include "ui_main.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>

#include "app/app_state.h"
#include "app/identity.h"
#include "app/usage_stats.h"
#include "assets/assets.h"
#include "assets/fonts/fonts_es.h"
#include "baby_widget.h"
#include "config.h"
#include "i18n.h"
#include "sound/sound.h"
#include "theme.h"

/* Portrait home layout (240x320):
 *   top bar   : wifi coverage | wordmark | gear
 *   centre    : animated baby + halo (or full-screen empty incubator)
 *   row       : 3 state icons (colour = state) + "hold my hand" button
 *   bottom    : always-visible status bar (IncuTwin state in words)    */
#define BABY_X 63              /* baby sprite is 114 px wide  */
#define BABY_Y 38              /* and 185 px tall             */
#define BABY_CX (BABY_X + 57)
#define BABY_CY (BABY_Y + 92)
#define ICON_D 44              /* state icon circle diameter  */
#define ICON_Y 232             /* icons + hand button row     */
#define STATUS_Y 288           /* status bar                  */

typedef enum {
    HALO_OFFLINE = 0,
    HALO_CALM,
    HALO_WARM,
    HALO_PHOTO,
    HALO_ALARM,
} halo_mode_t;

static lv_obj_t *scr_splash, *scr_home, *scr_settings;
static lv_obj_t *s_halo;
static lv_obj_t *s_empty_img, *s_parents_img, *s_wifi_img;
static lv_obj_t *s_icon_thermo, *s_icon_photo, *s_icon_heart;
static lv_obj_t *s_btn_hand, *s_lbl_hand;
static lv_obj_t *s_status, *s_lbl_status;
static halo_mode_t s_halo_mode = HALO_CALM;

/* settings widgets */
static lv_obj_t *s_btn_es, *s_btn_en;
static lv_obj_t *s_swatches[BABY_SKIN_TONE_COUNT];
static lv_obj_t *s_lbl_settings_title, *s_lbl_lang, *s_lbl_tone, *s_lbl_hint;
static lv_obj_t *s_lbl_back, *s_lbl_wifi, *s_lbl_cloud, *s_lbl_ver;
static lv_obj_t *s_lbl_sound, *s_btn_vol[4];
static const str_id_t VOL_STR[4] = {STR_VOL_OFF, STR_VOL_LOW, STR_VOL_MID,
                                    STR_VOL_HIGH};

static const uint32_t SWATCH_COLORS[BABY_SKIN_TONE_COUNT] = {
    0xF4C1A6, 0xE9AF8C, 0xD0946C, 0xAC704E, 0x865438, 0x5C3A28};

/* ------------------------------------------------------------------- halo */

static void halo_opa_cb(void *var, int32_t v) {
    lv_obj_set_style_bg_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void halo_start_anim(uint32_t period) {
    lv_anim_del(s_halo, halo_opa_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_halo);
    lv_anim_set_exec_cb(&a, halo_opa_cb);
    lv_anim_set_values(&a, LV_OPA_20, LV_OPA_60);
    lv_anim_set_time(&a, period);
    lv_anim_set_playback_time(&a, period);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void halo_set_mode(halo_mode_t m) {
    if (m == s_halo_mode) return;
    s_halo_mode = m;

    lv_color_t col = COL_HALO_CALM;
    uint32_t period = 2600;
    switch (m) {
        case HALO_OFFLINE:
            lv_obj_add_flag(s_halo, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(scr_home, COL_BG, 0);
            return;
        case HALO_WARM: col = COL_HALO_WARM; break;
        case HALO_PHOTO: col = COL_HALO_PHOTO; period = 2000; break;
        case HALO_ALARM: col = COL_HALO_ALARM; period = 700; break;
        default: break;
    }
    lv_obj_clear_flag(s_halo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(s_halo, col, 0);
    lv_obj_set_style_bg_color(
        scr_home, m == HALO_PHOTO ? lv_color_hex(0xE8F1FB) : COL_BG, 0);
    halo_start_anim(period);
}

/* ------------------------------------------------------------ state icons */

static lv_obj_t *state_icon_create(lv_obj_t *parent, lv_coord_t x,
                                   const void *icon_img) {
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, ICON_D, ICON_D);
    lv_obj_set_pos(c, x, ICON_Y);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c, COL_CARD, 0);
    lv_obj_set_style_border_width(c, 3, 0);
    lv_obj_set_style_border_color(c, COL_OFFLINE, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *icon = lv_img_create(c);
    lv_img_set_src(icon, icon_img);
    lv_obj_center(icon);
    return c;
}

static void state_icon_set(lv_obj_t *c, lv_color_t col) {
    lv_obj_set_style_border_color(c, col, 0);
}

/* ------------------------------------------------------------ navigation */

static void gear_clicked(lv_event_t *e) {
    (void)e;
    lv_scr_load_anim(scr_settings, LV_SCR_LOAD_ANIM_MOVE_LEFT, 220, 0, false);
}

/* mantener pulsado el engranaje -> confirmación de factory reset */
static void reset_msgbox_cb(lv_event_t *e) {
    lv_obj_t *mbox = lv_event_get_current_target(e);
    if (lv_msgbox_get_active_btn(mbox) == 0) prov_factory_reset();
    lv_msgbox_close(mbox);
}

static void gear_long_pressed(lv_event_t *e) {
    (void)e;
    static const char *btns[] = {"Reset", LV_SYMBOL_CLOSE, ""};
    lv_obj_t *m = lv_msgbox_create(nullptr, "IncuTwin",
                                   g_lang == 0 ? "¿Restablecer de fábrica?"
                                               : "Factory reset?",
                                   btns, false);
    lv_obj_center(m);
    lv_obj_add_event_cb(m, reset_msgbox_cb, LV_EVENT_VALUE_CHANGED, nullptr);
}

/* "Agarra mi mano": tocar al bebé lo despierta y cuenta como interacción */
static void baby_touched(lv_event_t *e) {
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_PRESSED) {
        usage_hand_begin();
        sound_hand_hold(true);
        baby_set_awake(true);
    } else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) {
        usage_hand_end();
        sound_hand_hold(false);
        state_lock();
        bool awake = g_state.awake;
        state_unlock();
        baby_set_awake(awake);
    }
}

static void back_clicked(lv_event_t *e) {
    (void)e;
    lv_scr_load_anim(scr_home, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 220, 0, false);
}

/* --------------------------------------------------------------- language */

static void update_texts(void);

static void lang_clicked(lv_event_t *e) {
    int lang = (int)(intptr_t)lv_event_get_user_data(e);
    i18n_set_lang(lang);
    update_texts();
}

static void vol_btns_refresh(void) {
    for (int i = 0; i < 4; i++) {
        bool sel = sound_get_volume() == (uint8_t)i;
        lv_obj_set_style_bg_color(s_btn_vol[i], sel ? COL_NAVY : COL_CARD, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(s_btn_vol[i], 0),
                                    sel ? lv_color_white() : COL_NAVY, 0);
    }
}

static void vol_clicked(lv_event_t *e) {
    uint8_t level = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (level == sound_get_volume()) return;
    sound_set_volume(level);
    vol_btns_refresh();
    if (level > 0) sound_play_test(); /* preview the new loudness */
}

static void lang_btns_refresh(void) {
    lv_obj_set_style_bg_color(s_btn_es, g_lang == 0 ? COL_NAVY : COL_CARD, 0);
    lv_obj_set_style_bg_color(s_btn_en, g_lang == 1 ? COL_NAVY : COL_CARD, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(s_btn_es, 0),
                                g_lang == 0 ? lv_color_white() : COL_NAVY, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(s_btn_en, 0),
                                g_lang == 1 ? lv_color_white() : COL_NAVY, 0);
}

/* ------------------------------------------------------------- skin tones */

static void swatch_refresh(void) {
    for (int i = 0; i < BABY_SKIN_TONE_COUNT; i++) {
        lv_obj_set_style_border_width(s_swatches[i],
                                      i == baby_get_skin_tone() ? 4 : 1, 0);
        lv_obj_set_style_border_color(
            s_swatches[i],
            i == baby_get_skin_tone() ? COL_NAVY : COL_OFFLINE, 0);
    }
}

/* Tono de piel: solo lectura — viene del país de la IncuNest asignada */

/* ---------------------------------------------------------------- screens */

static void build_splash(void) {
    scr_splash = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_splash, lv_color_white(), 0);

    lv_obj_t *logo = lv_img_create(scr_splash);
    lv_img_set_src(logo, &img_logo);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *spin = lv_spinner_create(scr_splash, 1000, 60);
    lv_obj_set_size(spin, 28, 28);
    lv_obj_align(spin, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_arc_color(spin, COL_CORAL, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spin, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spin, 4, LV_PART_MAIN);
}

static void build_home(void) {
    scr_home = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_home, COL_BG, 0);
    lv_obj_clear_flag(scr_home, LV_OBJ_FLAG_SCROLLABLE);

    /* full-screen "empty incubator" image, shown while offline
     * (first child = bottom of the z-order; the top bar stays above) */
    s_empty_img = lv_img_create(scr_home);
    lv_img_set_src(s_empty_img, &img_incunest_empty);
    lv_obj_set_pos(s_empty_img, 0, 0);
    lv_obj_add_flag(s_empty_img, LV_OBJ_FLAG_HIDDEN);

    /* full-screen "baby with parents", shown for a while after the baby
     * leaves the incubator (see PARENTS_MODE_MS) */
    s_parents_img = lv_img_create(scr_home);
    lv_img_set_src(s_parents_img, &img_baby_parents);
    lv_obj_set_pos(s_parents_img, 0, 0);
    lv_obj_add_flag(s_parents_img, LV_OBJ_FLAG_HIDDEN);

    /* breathing halo behind the baby */
    s_halo = lv_obj_create(scr_home);
    lv_obj_set_size(s_halo, 190, 190);
    lv_obj_set_style_radius(s_halo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_halo, COL_HALO_CALM, 0);
    lv_obj_set_style_bg_opa(s_halo, LV_OPA_30, 0);
    lv_obj_set_style_border_width(s_halo, 0, 0);
    lv_obj_set_pos(s_halo, BABY_CX - 95, BABY_CY - 95);
    lv_obj_clear_flag(s_halo, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    halo_start_anim(2600);

    baby_widget_create(scr_home, BABY_X, BABY_Y);
    lv_obj_add_flag(baby_widget_obj(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(baby_widget_obj(), baby_touched, LV_EVENT_ALL,
                        nullptr);

    /* state icons: thermo, lamp, heart (colour = state, no text) */
    s_icon_thermo = state_icon_create(scr_home, 8, &img_icon_thermo);
    s_icon_photo = state_icon_create(scr_home, 58, &img_icon_lamp);
    s_icon_heart = state_icon_create(scr_home, 108, &img_heart);

    /* "Agarra mi mano": same interaction as touching the baby */
    s_btn_hand = lv_btn_create(scr_home);
    lv_obj_set_size(s_btn_hand, 74, ICON_D);
    lv_obj_set_pos(s_btn_hand, 158, ICON_Y);
    lv_obj_set_style_bg_color(s_btn_hand, COL_CORAL, 0);
    lv_obj_set_style_radius(s_btn_hand, 14, 0);
    lv_obj_set_style_shadow_width(s_btn_hand, 0, 0);
    lv_obj_add_event_cb(s_btn_hand, baby_touched, LV_EVENT_ALL, nullptr);
    s_lbl_hand = lv_label_create(s_btn_hand);
    lv_label_set_text(s_lbl_hand, tr(STR_HAND));
    lv_obj_set_style_text_font(s_lbl_hand, &lv_font_es_12, 0);
    lv_obj_set_style_text_align(s_lbl_hand, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_lbl_hand, 66);
    lv_label_set_long_mode(s_lbl_hand, LV_LABEL_LONG_WRAP);
    lv_obj_center(s_lbl_hand);

    /* always-visible status bar: the IncuTwin state in words */
    s_status = lv_obj_create(scr_home);
    lv_obj_set_size(s_status, 224, 26);
    lv_obj_set_pos(s_status, 8, STATUS_Y);
    lv_obj_set_style_radius(s_status, 10, 0);
    lv_obj_set_style_bg_color(s_status, COL_CARD, 0);
    lv_obj_set_style_border_width(s_status, 2, 0);
    lv_obj_set_style_border_color(s_status, COL_OFFLINE, 0);
    lv_obj_set_style_pad_all(s_status, 0, 0);
    lv_obj_clear_flag(s_status, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    s_lbl_status = lv_label_create(s_status);
    lv_label_set_text(s_lbl_status, tr(STR_ST_CONNECTING));
    lv_obj_set_style_text_font(s_lbl_status, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_lbl_status, COL_OFFLINE, 0);
    lv_obj_center(s_lbl_status);

    /* top bar: wifi coverage | wordmark | gear */
    s_wifi_img = lv_img_create(scr_home);
    lv_img_set_src(s_wifi_img, &img_wifi_off);
    lv_obj_set_pos(s_wifi_img, 8, 8);

    lv_obj_t *wm = lv_img_create(scr_home);
    lv_img_set_src(wm, &img_wordmark);
    lv_obj_align(wm, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *gear = lv_btn_create(scr_home);
    lv_obj_set_size(gear, 42, 30);
    lv_obj_align(gear, LV_ALIGN_TOP_RIGHT, -4, 5);
    lv_obj_set_style_bg_color(gear, COL_CARD, 0);
    lv_obj_set_style_shadow_width(gear, 0, 0);
    lv_obj_set_style_radius(gear, 10, 0);
    lv_obj_add_event_cb(gear, gear_clicked, LV_EVENT_SHORT_CLICKED, nullptr);
    lv_obj_add_event_cb(gear, gear_long_pressed, LV_EVENT_LONG_PRESSED,
                        nullptr);
    lv_obj_t *gl = lv_label_create(gear);
    lv_label_set_text(gl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gl, COL_NAVY, 0);
    lv_obj_center(gl);
}

static void build_settings(void) {
    scr_settings = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_settings, COL_BG, 0);

    /* header: back + title */
    lv_obj_t *back = lv_btn_create(scr_settings);
    lv_obj_set_size(back, 96, 40);
    lv_obj_set_pos(back, 6, 6);
    lv_obj_set_style_bg_color(back, COL_NAVY, 0);
    lv_obj_set_style_radius(back, 12, 0);
    lv_obj_add_event_cb(back, back_clicked, LV_EVENT_CLICKED, nullptr);
    s_lbl_back = lv_label_create(back);
    lv_label_set_text_fmt(s_lbl_back, LV_SYMBOL_LEFT " %s", tr(STR_BACK));
    lv_obj_center(s_lbl_back);

    s_lbl_settings_title = lv_label_create(scr_settings);
    lv_label_set_text(s_lbl_settings_title, tr(STR_SETTINGS));
    lv_obj_set_style_text_font(s_lbl_settings_title, &lv_font_es_20, 0);
    lv_obj_set_style_text_color(s_lbl_settings_title, COL_NAVY, 0);
    lv_obj_align(s_lbl_settings_title, LV_ALIGN_TOP_RIGHT, -10, 14);

    /* language */
    s_lbl_lang = lv_label_create(scr_settings);
    lv_label_set_text(s_lbl_lang, tr(STR_LANGUAGE));
    lv_obj_set_style_text_color(s_lbl_lang, COL_NAVY_DARK, 0);
    lv_obj_set_pos(s_lbl_lang, 12, 56);

    s_btn_es = lv_btn_create(scr_settings);
    lv_obj_set_size(s_btn_es, 104, 40);
    lv_obj_set_pos(s_btn_es, 12, 76);
    lv_obj_set_style_radius(s_btn_es, 12, 0);
    lv_obj_set_style_border_width(s_btn_es, 2, 0);
    lv_obj_set_style_border_color(s_btn_es, COL_NAVY, 0);
    lv_obj_set_style_shadow_width(s_btn_es, 0, 0);
    lv_obj_add_event_cb(s_btn_es, lang_clicked, LV_EVENT_CLICKED, (void *)0);
    lv_obj_t *l = lv_label_create(s_btn_es);
    lv_label_set_text(l, "Español");
    lv_obj_set_style_text_font(l, &lv_font_es_16, 0);
    lv_obj_center(l);

    s_btn_en = lv_btn_create(scr_settings);
    lv_obj_set_size(s_btn_en, 104, 40);
    lv_obj_set_pos(s_btn_en, 124, 76);
    lv_obj_set_style_radius(s_btn_en, 12, 0);
    lv_obj_set_style_border_width(s_btn_en, 2, 0);
    lv_obj_set_style_border_color(s_btn_en, COL_NAVY, 0);
    lv_obj_set_style_shadow_width(s_btn_en, 0, 0);
    lv_obj_add_event_cb(s_btn_en, lang_clicked, LV_EVENT_CLICKED, (void *)1);
    l = lv_label_create(s_btn_en);
    lv_label_set_text(l, "English");
    lv_obj_center(l);

    /* skin tone */
    s_lbl_tone = lv_label_create(scr_settings);
    lv_label_set_text(s_lbl_tone, tr(STR_SKIN_TONE));
    lv_obj_set_style_text_color(s_lbl_tone, COL_NAVY_DARK, 0);
    lv_obj_set_pos(s_lbl_tone, 12, 124);

    for (int i = 0; i < BABY_SKIN_TONE_COUNT; i++) {
        s_swatches[i] = lv_obj_create(scr_settings);
        lv_obj_set_size(s_swatches[i], 34, 34);
        lv_obj_set_pos(s_swatches[i], 12 + i * 37, 144);
        lv_obj_set_style_radius(s_swatches[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_swatches[i],
                                  lv_color_hex(SWATCH_COLORS[i]), 0);
        lv_obj_set_style_shadow_width(s_swatches[i], 0, 0);
        lv_obj_clear_flag(s_swatches[i],
                          LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }

    s_lbl_hint = lv_label_create(scr_settings);
    lv_label_set_text(s_lbl_hint, tr(STR_SKIN_HINT));
    lv_obj_set_style_text_color(s_lbl_hint, COL_OFFLINE, 0);
    lv_obj_set_style_text_font(s_lbl_hint, &lv_font_es_12, 0);
    lv_obj_set_pos(s_lbl_hint, 12, 182);
    lv_obj_set_width(s_lbl_hint, 216);
    lv_label_set_long_mode(s_lbl_hint, LV_LABEL_LONG_WRAP);

    /* sound volume */
    s_lbl_sound = lv_label_create(scr_settings);
    lv_label_set_text(s_lbl_sound, tr(STR_SOUND));
    lv_obj_set_style_text_color(s_lbl_sound, COL_NAVY_DARK, 0);
    lv_obj_set_pos(s_lbl_sound, 12, 214);

    static const int16_t VOL_X[4] = {12, 74, 124, 182};
    static const int16_t VOL_W[4] = {56, 44, 52, 44};
    for (int i = 0; i < 4; i++) {
        s_btn_vol[i] = lv_btn_create(scr_settings);
        lv_obj_set_size(s_btn_vol[i], VOL_W[i], 32);
        lv_obj_set_pos(s_btn_vol[i], VOL_X[i], 234);
        lv_obj_set_style_radius(s_btn_vol[i], 10, 0);
        lv_obj_set_style_border_width(s_btn_vol[i], 2, 0);
        lv_obj_set_style_border_color(s_btn_vol[i], COL_NAVY, 0);
        lv_obj_set_style_shadow_width(s_btn_vol[i], 0, 0);
        lv_obj_add_event_cb(s_btn_vol[i], vol_clicked, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        lv_obj_t *vl = lv_label_create(s_btn_vol[i]);
        lv_label_set_text(vl, tr(VOL_STR[i]));
        lv_obj_set_style_text_font(vl, &lv_font_es_12, 0);
        lv_obj_center(vl);
    }

    /* info footer (stacked: the portrait screen is only 240 px wide) */
    s_lbl_wifi = lv_label_create(scr_settings);
    lv_obj_set_style_text_font(s_lbl_wifi, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(s_lbl_wifi, COL_NAVY_DARK, 0);
    lv_obj_align(s_lbl_wifi, LV_ALIGN_BOTTOM_LEFT, 12, -36);

    s_lbl_cloud = lv_label_create(scr_settings);
    lv_obj_set_style_text_font(s_lbl_cloud, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(s_lbl_cloud, COL_NAVY_DARK, 0);
    lv_obj_align(s_lbl_cloud, LV_ALIGN_BOTTOM_LEFT, 12, -20);

    s_lbl_ver = lv_label_create(scr_settings);
    lv_label_set_text_fmt(s_lbl_ver, "%s · v%s", identity_sn(), FW_VERSION);
    lv_obj_set_style_text_font(s_lbl_ver, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(s_lbl_ver, COL_OFFLINE, 0);
    lv_obj_align(s_lbl_ver, LV_ALIGN_BOTTOM_LEFT, 12, -4);

    lang_btns_refresh();
    swatch_refresh();
    vol_btns_refresh();
}

/* ------------------------------------------------------------ translation */

static void update_texts(void) {
    lv_label_set_text(s_lbl_hand, tr(STR_HAND));
    lv_label_set_text(s_lbl_settings_title, tr(STR_SETTINGS));
    lv_label_set_text(s_lbl_lang, tr(STR_LANGUAGE));
    lv_label_set_text(s_lbl_tone, tr(STR_SKIN_TONE));
    lv_label_set_text(s_lbl_hint, tr(STR_SKIN_HINT));
    lv_label_set_text(s_lbl_sound, tr(STR_SOUND));
    for (int i = 0; i < 4; i++)
        lv_label_set_text(lv_obj_get_child(s_btn_vol[i], 0), tr(VOL_STR[i]));
    lv_label_set_text_fmt(s_lbl_back, LV_SYMBOL_LEFT " %s", tr(STR_BACK));
    lang_btns_refresh();
    state_lock();
    g_state_dirty = true; /* force mood/footer re-render in new language */
    state_unlock();
}

/* ----------------------------------------------------------- state -> UI */

static void wifi_icon_update(const twin_state_t *st) {
    if (!st->wifi_connected) {
        lv_img_set_src(s_wifi_img, &img_wifi_off);
        lv_obj_set_style_img_recolor_opa(s_wifi_img, LV_OPA_TRANSP, 0);
        return;
    }
    static const lv_img_dsc_t *LVL[] = {&img_wifi_0, &img_wifi_1, &img_wifi_2,
                                        &img_wifi_3};
    int rssi = WiFi.RSSI();
    int lvl = rssi >= -55 ? 3 : rssi >= -67 ? 2 : rssi >= -78 ? 1 : 0;
    lv_img_set_src(s_wifi_img, LVL[lvl]);
    /* amber tint: WiFi up but the cloud stream is down */
    if (st->cloud_connected) {
        lv_obj_set_style_img_recolor_opa(s_wifi_img, LV_OPA_TRANSP, 0);
    } else {
        lv_obj_set_style_img_recolor(s_wifi_img, COL_WARM, 0);
        lv_obj_set_style_img_recolor_opa(s_wifi_img, LV_OPA_COVER, 0);
    }
}

/* what fills the centre of the home screen */
typedef enum {
    VIEW_EMPTY = 0, /* empty incubator (offline / no baby) */
    VIEW_BABY,      /* animated baby + icons + hand button */
    VIEW_PARENTS,   /* baby out with parents               */
} home_view_t;

static void home_set_view(home_view_t view) {
    lv_obj_t *widgets[] = {baby_widget_obj(), s_icon_thermo, s_icon_photo,
                           s_icon_heart, s_btn_hand};
    for (auto *w : widgets) {
        if (view == VIEW_BABY)
            lv_obj_clear_flag(w, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(w, LV_OBJ_FLAG_HIDDEN);
    }
    if (view == VIEW_EMPTY)
        lv_obj_clear_flag(s_empty_img, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(s_empty_img, LV_OBJ_FLAG_HIDDEN);
    if (view == VIEW_PARENTS)
        lv_obj_clear_flag(s_parents_img, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(s_parents_img, LV_OBJ_FLAG_HIDDEN);
}

static void status_bar_set(str_id_t sid, lv_color_t col) {
    lv_label_set_text(s_lbl_status, tr(sid));
    lv_obj_set_style_text_color(s_lbl_status, col, 0);
    lv_obj_set_style_border_color(s_status, col, 0);
}

static void ui_apply_state(void) {
    state_lock();
    twin_state_t st = g_state;
    g_state_dirty = false;
    state_unlock();

    bool stale = st.last_update_ms == 0 ||
                 (millis() - st.last_update_ms) > (DATA_STALE_S * 1000UL);
    bool online = st.wifi_connected && st.cloud_connected && st.node_seen &&
                  st.incubator_online && !stale;
    bool show_baby = online && st.baby_present;

    /* traza cada transicion del bebe con los flags que la explican */
    static int prev_show = -1;
    if ((int)show_baby != prev_show) {
        prev_show = show_baby;
        Serial.printf("[ui] show_baby=%d wifi=%d cloud=%d node=%d inc_on=%d "
                      "stale=%d baby=%d hr=%u thermo=%d age_ms=%lu t=%lu\n",
                      show_baby, st.wifi_connected, st.cloud_connected,
                      st.node_seen, st.incubator_online, stale,
                      st.baby_present, st.heart_rate, (int)st.thermo,
                      (unsigned long)(millis() - st.last_update_ms),
                      (unsigned long)millis());
    }

    /* ventana "con sus papás": cualquier caída de show_baby la abre;
     * el bebé de vuelta la cancela. Resta con signo por el rollover. */
    static bool prev_baby = false;
    static uint32_t parents_until = 0; /* 0 = inactiva */
    if (!show_baby && prev_baby) {
        parents_until = millis() + PARENTS_MODE_MS;
        if (parents_until == 0) parents_until = 1;
        sound_play_parents();
    } else if (show_baby) {
        parents_until = 0;
    }
    prev_baby = show_baby;
    if (parents_until != 0 && (int32_t)(millis() - parents_until) >= 0)
        parents_until = 0; /* expirada: que no re-arme tras el wrap */
    bool parents_active = parents_until != 0 &&
                          (int32_t)(millis() - parents_until) < 0;

    wifi_icon_update(&st);
    home_set_view(parents_active ? VIEW_PARENTS
                                 : show_baby ? VIEW_BABY : VIEW_EMPTY);
    sound_on_state(&st);

    /* status bar: one line that always tells the IncuTwin state */
    if (parents_active)
        status_bar_set(STR_ST_PARENTS, COL_OK);
    else if (!st.wifi_connected)
        status_bar_set(STR_ST_NO_WIFI, COL_RED);
    else if (!st.cloud_connected)
        status_bar_set(STR_ST_CONNECTING, COL_WARM);
    else if (!st.node_seen)
        status_bar_set(STR_ST_UNLINKED, COL_OFFLINE);
    else if (!online)
        status_bar_set(STR_ST_OFF, COL_OFFLINE);
    else if (!st.baby_present)
        status_bar_set(STR_ST_NO_BABY, COL_PHOTO);
    else if (st.thermo == THERMO_ALARM)
        status_bar_set(STR_ALARM, COL_RED);
    else
        status_bar_set(st.awake ? STR_ST_BABY_AWAKE : STR_ST_BABY_SLEEP,
                       COL_OK);

    /* thermoregulation icon */
    if (!online) {
        state_icon_set(s_icon_thermo, COL_OFFLINE);
    } else {
        switch (st.thermo) {
            case THERMO_HEATING: state_icon_set(s_icon_thermo, COL_WARM); break;
            case THERMO_STABLE: state_icon_set(s_icon_thermo, COL_OK); break;
            case THERMO_ALARM: state_icon_set(s_icon_thermo, COL_RED); break;
            default: state_icon_set(s_icon_thermo, COL_OFFLINE);
        }
    }

    /* phototherapy icon */
    state_icon_set(s_icon_photo,
                   online && st.phototherapy ? COL_PHOTO : COL_OFFLINE);

    /* heart icon */
    state_icon_set(s_icon_heart,
                   online && st.heart_rate > 0 ? COL_CORAL : COL_OFFLINE);

    /* baby */
    baby_set_skin_tone(st.skin_tone);
    baby_set_awake(st.awake);
    baby_set_heart_rate(show_baby ? st.heart_rate : 0);
    swatch_refresh();

    /* halo / lighting */
    if (!show_baby)
        halo_set_mode(HALO_OFFLINE);
    else if (st.thermo == THERMO_ALARM)
        halo_set_mode(HALO_ALARM);
    else if (st.phototherapy)
        halo_set_mode(HALO_PHOTO);
    else if (st.thermo == THERMO_HEATING)
        halo_set_mode(HALO_WARM);
    else
        halo_set_mode(HALO_CALM);

    /* settings info */
    if (lv_scr_act() == scr_settings) {
        if (st.wifi_connected)
            lv_label_set_text_fmt(s_lbl_wifi, "%s: %s", tr(STR_WIFI),
                                  WiFi.localIP().toString().c_str());
        else
            lv_label_set_text_fmt(s_lbl_wifi, "%s: %s", tr(STR_WIFI),
                                  tr(STR_NO_CONNECTION));
        lv_label_set_text_fmt(s_lbl_cloud, "%s: %s", tr(STR_CLOUD),
                              st.cloud_connected ? tr(STR_CONNECTED)
                                                 : tr(STR_NO_CONNECTION));
    }
}

static void refresh_timer_cb(lv_timer_t *t) {
    (void)t;
    static uint32_t last_forced = 0;
    if (g_state_dirty || millis() - last_forced > 1000) {
        last_forced = millis();
        ui_apply_state();
    }
}

static void splash_done_cb(lv_timer_t *t) {
    lv_scr_load_anim(scr_home, LV_SCR_LOAD_ANIM_FADE_ON, 400, 0, false);
    lv_timer_del(t);
}

/* -------------------------------------------------------------------- init */

void ui_init(void) {
    i18n_load();

    build_splash();
    build_home();
    build_settings();

    lv_disp_load_scr(scr_splash);
    lv_timer_create(splash_done_cb, SPLASH_MS, nullptr);
    lv_timer_create(refresh_timer_cb, 250, nullptr);
    ui_apply_state();
}
