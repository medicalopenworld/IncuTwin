#include "onboarding.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>

#include "app/identity.h"
#include "assets/assets.h"
#include "assets/fonts/fonts_es.h"
#include "config.h"
#include "i18n.h"
#include "net/portal.h"
#include "net/tb_client.h"
#include "theme.h"

typedef enum {
    OB_LANG = 0,
    OB_CONNECT,
    OB_WIFI_TRY,
    OB_REGISTER,
    OB_PAIR,
} ob_step_t;

static ob_step_t s_step = OB_LANG;
static lv_obj_t *s_scr = nullptr;
static lv_timer_t *s_timer = nullptr;
static uint32_t s_step_t0 = 0;
static bool s_wifi_failed = false;

static void show_step(ob_step_t step);

/* ------------------------------------------------------------------ helpers */

static lv_obj_t *new_screen(void) {
    lv_obj_t *old_scr = s_scr;
    s_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    /* cargar antes de borrar: lv_scr_load_anim() toca la pantalla activa
     * anterior internamente, así que borrarla antes dejaría un puntero
     * colgante y provocaría un crash (LoadProhibited). */
    lv_scr_load(s_scr);
    if (old_scr) lv_obj_del(old_scr);
    return s_scr;
}

static lv_obj_t *title_label(lv_obj_t *parent, const char *txt) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_es_20, 0);
    lv_obj_set_style_text_color(l, COL_NAVY, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 10);
    return l;
}

static lv_obj_t *make_qr(lv_obj_t *parent, const char *data,
                         lv_coord_t size) {
    lv_obj_t *qr =
        lv_qrcode_create(parent, size, lv_color_black(), lv_color_white());
    lv_qrcode_update(qr, data, strlen(data));
    lv_obj_set_style_border_width(qr, 6, 0);
    lv_obj_set_style_border_color(qr, lv_color_white(), 0);
    return qr;
}

/* -------------------------------------------------------------------- steps */

static void lang_clicked(lv_event_t *e) {
    i18n_set_lang((int)(intptr_t)lv_event_get_user_data(e));
    show_step(OB_CONNECT);
}

static void build_lang(void) {
    lv_obj_t *scr = new_screen();

    lv_obj_t *logo = lv_img_create(scr);
    lv_img_set_src(logo, &img_wordmark);
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 14);

    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, "Elige tu idioma /\nChoose your language");
    lv_obj_set_style_text_color(t, COL_NAVY_DARK, 0);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -70);

    const char *names[2] = {"Español", "English"};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *b = lv_btn_create(scr);
        lv_obj_set_size(b, 150, 52);
        lv_obj_align(b, LV_ALIGN_CENTER, 0, i == 0 ? 0 : 66);
        lv_obj_set_style_radius(b, 14, 0);
        lv_obj_set_style_bg_color(b, i == 0 ? COL_NAVY : COL_CARD, 0);
        lv_obj_set_style_border_width(b, 2, 0);
        lv_obj_set_style_border_color(b, COL_NAVY, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_add_event_cb(b, lang_clicked, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, names[i]);
        lv_obj_set_style_text_color(l, i == 0 ? lv_color_white() : COL_NAVY,
                                    0);
        lv_obj_set_style_text_font(l, &lv_font_es_20, 0);
        lv_obj_center(l);
    }
}

static void build_connect(void) {
    portal_start();
    lv_obj_t *scr = new_screen();
    title_label(scr, tr(STR_OB_CONNECT_TITLE));

    char wifi_qr[96];
    snprintf(wifi_qr, sizeof(wifi_qr), "WIFI:T:WPA;S:%s;P:%s;;",
             portal_ap_ssid(), AP_PASSWORD);
    lv_obj_t *qr = make_qr(scr, wifi_qr, 130);
    lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 42);

    lv_obj_t *steps = lv_label_create(scr);
    lv_label_set_text(steps, tr(STR_OB_CONNECT_STEPS));
    lv_obj_set_style_text_color(steps, COL_NAVY_DARK, 0);
    lv_obj_set_width(steps, 224);
    lv_label_set_long_mode(steps, LV_LABEL_LONG_WRAP);
    lv_obj_align(steps, LV_ALIGN_TOP_MID, 0, 186);

    lv_obj_t *net = lv_label_create(scr);
    lv_label_set_text_fmt(net, "%s: %s · %s: %s", tr(STR_OB_NETWORK),
                          portal_ap_ssid(), "Pass", AP_PASSWORD);
    lv_obj_set_style_text_color(net, COL_OFFLINE, 0);
    lv_obj_set_style_text_font(net, &lv_font_es_12, 0);
    lv_obj_align(net, LV_ALIGN_BOTTOM_MID, 0, -28);

    if (s_wifi_failed) {
        lv_obj_t *err = lv_label_create(scr);
        lv_label_set_text(err, tr(STR_OB_WIFI_FAIL));
        lv_obj_set_style_text_color(err, COL_RED, 0);
        lv_obj_set_style_text_font(err, &lv_font_es_12, 0);
        lv_obj_align(err, LV_ALIGN_BOTTOM_MID, 0, -6);
    }
}

static void build_progress(const char *txt) {
    lv_obj_t *scr = new_screen();
    lv_obj_t *spin = lv_spinner_create(scr, 1000, 60);
    lv_obj_set_size(spin, 60, 60);
    lv_obj_align(spin, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_arc_color(spin, COL_CORAL, LV_PART_INDICATOR);

    lv_obj_t *l = lv_label_create(scr);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, COL_NAVY_DARK, 0);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, 40);
}

static void finish_clicked(lv_event_t *e) {
    (void)e;
    prov_mark_done();
    ESP.restart();
}

static void build_pair(void) {
    lv_obj_t *scr = new_screen();
    title_label(scr, tr(STR_OB_PAIR_TITLE));

    char url[160];
    snprintf(url, sizeof(url), PAIR_URL_FMT, identity_sn(), prov_pair_code());
    lv_obj_t *qr = make_qr(scr, url, 130);
    lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 42);

    lv_obj_t *steps = lv_label_create(scr);
    lv_label_set_text(steps, tr(STR_OB_PAIR_STEPS));
    lv_obj_set_style_text_color(steps, COL_NAVY_DARK, 0);
    lv_obj_set_width(steps, 224);
    lv_label_set_long_mode(steps, LV_LABEL_LONG_WRAP);
    lv_obj_align(steps, LV_ALIGN_TOP_MID, 0, 186);

    lv_obj_t *sn = lv_label_create(scr);
    lv_label_set_text_fmt(sn, "SN: %s", identity_sn());
    lv_obj_set_style_text_color(sn, COL_OFFLINE, 0);
    lv_obj_set_style_text_font(sn, &lv_font_es_12, 0);
    lv_obj_align(sn, LV_ALIGN_BOTTOM_LEFT, 12, -22);

    lv_obj_t *b = lv_btn_create(scr);
    lv_obj_set_size(b, 110, 44);
    lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, COL_NAVY, 0);
    lv_obj_add_event_cb(b, finish_clicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, tr(STR_OB_FINISH));
    lv_obj_center(l);
}

/* ------------------------------------------------------------ state machine */

static void show_step(ob_step_t step) {
    s_step = step;
    s_step_t0 = millis();
    switch (step) {
        case OB_LANG: build_lang(); break;
        case OB_CONNECT: build_connect(); break;
        case OB_WIFI_TRY: {
            build_progress(tr(STR_OB_CONNECTING));
            char ssid[33] = {0}, pass[65] = {0};
            prov_get_wifi(ssid, sizeof(ssid), pass, sizeof(pass));
            WiFi.begin(ssid, pass);
            break;
        }
        case OB_REGISTER: build_progress(tr(STR_OB_REGISTERING)); break;
        case OB_PAIR: build_pair(); break;
    }
}

static void ob_timer_cb(lv_timer_t *t) {
    (void)t;
    switch (s_step) {
        case OB_CONNECT:
            if (portal_state() == PORTAL_SUBMITTED) {
                s_wifi_failed = false;
                show_step(OB_WIFI_TRY);
            }
            break;

        case OB_WIFI_TRY:
            if (WiFi.status() == WL_CONNECTED) {
                portal_stop();
                show_step(OB_REGISTER);
            } else if (millis() - s_step_t0 > 25000) {
                s_wifi_failed = true;
                WiFi.disconnect();
                show_step(OB_CONNECT); /* portal sigue activo */
            }
            break;

        case OB_REGISTER:
            /* la tarea de ThingsBoard se provisiona sola al ver WiFi */
            if (tb_has_token() || millis() - s_step_t0 > 30000)
                show_step(OB_PAIR);
            break;

        default:
            break;
    }
}

void ui_onboarding_start(void) {
    i18n_load(); /* ES por defecto */
    show_step(OB_LANG);
    s_timer = lv_timer_create(ob_timer_cb, 400, nullptr);
    (void)s_timer;
}
