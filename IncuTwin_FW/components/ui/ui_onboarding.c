#include "ui_onboarding.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "app_events.h"
#include "assets.h"
#include "captive_portal.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_system.h"
#include "identity.h"
#include "net_wifi.h"
#include "settings.h"
#include "telemetry.h"
#include "assets_nowifi.h"
#include "ui_internal.h"
#include "ui_toy.h"
#include "ui_nowifi.h"

static const char *TAG = "onboarding";

#define WIFI_TIMEOUT_MS   25000
#define SERVER_TIMEOUT_MS 30000

/* Sin paso de idioma: arranca en español (settings) y se cambia en Ajustes */
typedef enum { OB_CONNECT = 0, OB_WIFI_TRY, OB_SERVER, OB_DONE } ob_step_t;

static ob_step_t s_step;
static lv_obj_t *s_scr;
static uint32_t s_step_t0;
static bool s_wifi_failed;
static bool s_wifi_only;      /* modo "cambiar WiFi" desde la pantalla principal */
static bool s_connect_pending; /* wifi_only: esperando a soltar la red vieja antes de probar la nueva */
static atomic_bool s_wifi_up = false;
static atomic_bool s_mqtt_up = false;
/* Probando la WiFi: el bebe de la antena busca senal y, al conectar, celebra (ui_nowifi.c).
 * Aun no se sabe el sexo del bebe: el dibujo de siempre (nino). */
static ui_nowifi_t *s_nw;
static bool s_celebrating;

static void show_step(ob_step_t step);
static void cancel_clicked(lv_event_t *e);

/* ----------------------------------------------------------------- helpers */

static lv_obj_t *new_screen(void)
{
    lv_obj_t *old = s_scr;
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_scrollable(s_scr, false);
    /* cargar antes de borrar la anterior: la pantalla activa no se puede destruir */
    lv_screen_load(s_scr);
    if (old) lv_obj_delete(old);
    return s_scr;
}

static lv_obj_t *title_label(lv_obj_t *parent, const char *txt)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_title_20, 0);
    lv_obj_set_style_text_color(l, COL_INK, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 10);
    return l;
}

static lv_obj_t *body_label(lv_obj_t *parent, const char *txt, int32_t y, const lv_font_t *font,
                            lv_color_t col)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(l, 224);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
    return l;
}

static lv_obj_t *make_qr(lv_obj_t *parent, const char *data, int32_t size, int32_t y)
{
    lv_obj_t *qr = lv_qrcode_create(parent);
    lv_qrcode_set_size(qr, size);
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_update(qr, data, strlen(data));
    lv_obj_set_style_border_width(qr, 6, 0); /* margen blanco del QR (que se lea) */
    lv_obj_set_style_border_color(qr, lv_color_white(), 0);
    lv_obj_set_style_outline_width(qr, TOY_BORDER, 0); /* y el marco de juguete */
    lv_obj_set_style_outline_color(qr, COL_INK, 0);
    lv_obj_set_style_radius(qr, 4, 0);
    toy_shadow(qr, TOY_SHADOW_M + TOY_BORDER); /* desde el QR: que asome 3 px por fuera del marco */
    lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, y);
    return qr;
}

static lv_obj_t *primary_button(lv_obj_t *parent, const char *txt, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, 150, 48);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    toy_button(b, true); /* boton principal coral de la app */
    lv_obj_set_style_text_font(l, &lv_font_title_16, 0);
    lv_obj_center(l);
    return b;
}

static void progress_screen(const char *txt)
{
    lv_obj_t *scr = new_screen();
    lv_obj_t *spin = lv_spinner_create(scr);
    lv_spinner_set_anim_params(spin, 1000, 60);
    lv_obj_set_size(spin, 60, 60);
    lv_obj_align(spin, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_arc_color(spin, COL_CORAL, LV_PART_INDICATOR);
    body_label(scr, txt, 190, &lv_font_es_16, COL_NAVY_DARK);
}

/* ------------------------------------------------------------------- steps */

/* "Conectando a tu WiFi...": el bebe saca la antena y busca senal (texto debajo) */
static void build_wifi_try(void)
{
    lv_obj_t *scr = new_screen();
    s_celebrating = false;
    s_nw = ui_nowifi_create(scr, (240 - NOWIFI_W) / 2, 40, 40 + NOWIFI_H + 8);
    ui_nowifi_search(s_nw, false);
}

static void build_connect(void)
{
    if (portal_start() != ESP_OK) {
        ESP_LOGE(TAG, "no se pudo arrancar el portal");
    }
    lv_obj_t *scr = new_screen();
    title_label(scr, tr(STR_OB_CONNECT_TITLE));

    char wifi_qr[96];
    snprintf(wifi_qr, sizeof(wifi_qr), "WIFI:T:WPA;S:%s;P:%s;;", portal_ap_ssid(), portal_ap_password());
    make_qr(scr, wifi_qr, 130, 42);
    body_label(scr, tr(STR_OB_CONNECT_STEPS), 186, &lv_font_es_14, COL_NAVY_DARK);

    char net[64];
    snprintf(net, sizeof(net), "%s: %s · Pass: %s", tr(STR_OB_NETWORK), portal_ap_ssid(),
             portal_ap_password());
    lv_obj_t *l = body_label(scr, net, 266, &lv_font_es_12, COL_MUTED);
    (void)l;
    if (s_wifi_failed) {
        body_label(scr, tr(STR_OB_WIFI_FAIL), 290, &lv_font_es_12, COL_INK); /* tinta: que no parezca una alarma */
    }
    if (s_wifi_only) {
        lv_obj_t *b = primary_button(scr, tr(STR_OB_CANCEL), cancel_clicked, NULL);
        lv_obj_set_size(b, 110, 36);
        toy_button(b, false);
        lv_obj_align(b, LV_ALIGN_TOP_RIGHT, -8, 230);
    }
}

static void finish_clicked(lv_event_t *e)
{
    (void)e;
    identity_prov_mark_done();
    ESP_LOGI(TAG, "onboarding completado: reiniciando");
    esp_restart();
}

/* Cambiar WiFi cancelado: el reinicio vuelve a la red guardada sin estados a medias. */
static void cancel_clicked(lv_event_t *e)
{
    (void)e;
    portal_stop();
    ESP_LOGI(TAG, "cambio de WiFi cancelado: reiniciando");
    esp_restart();
}

static void restart_timer_cb(lv_timer_t *t)
{
    (void)t;
    esp_restart();
}

static void build_done(void)
{
    lv_obj_t *scr = new_screen();
    title_label(scr, tr(STR_OB_DONE_TITLE));
    make_qr(scr, identity_sn(), 110, 42);
    char sn[40];
    snprintf(sn, sizeof(sn), "SN %s", identity_sn());
    body_label(scr, sn, 160, &lv_font_es_14, COL_NAVY);
    body_label(scr, tr(STR_OB_DONE_TEXT), 184, &lv_font_es_12, COL_NAVY_DARK);
    lv_obj_t *b = primary_button(scr, tr(STR_OB_FINISH), finish_clicked, NULL);
    lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -12);
}

static void show_step(ob_step_t step)
{
    static const char *const tel_step[] = { "connect", "wifi_try", "server", "done" };
    char d[32];
    snprintf(d, sizeof d, "{\"step\":\"%s\"}", tel_step[step]);
    telemetry_log("onboard", d);
    s_step = step;
    s_step_t0 = lv_tick_get();
    switch (step) {
    case OB_CONNECT: build_connect(); break;
    case OB_WIFI_TRY: {
        build_wifi_try();
        atomic_store(&s_wifi_up, false);
        if (s_wifi_only) {
            /* soltar la red actual y dejar 1,5 s antes de probar la nueva */
            net_wifi_disconnect();
            s_connect_pending = true;
            break;
        }
        char ssid[33], pass[65];
        if (identity_wifi_creds(ssid, sizeof(ssid), pass, sizeof(pass))) {
            net_wifi_connect(ssid, pass);
        }
        break;
    }
    case OB_SERVER:  progress_screen(tr(STR_OB_CONNECTING_SERVER)); break;
    case OB_DONE:    build_done(); break;
    }
}

/* ------------------------------------------------------------- state machine */

static void celebrated(void);

static void ob_timer_cb(lv_timer_t *t)
{
    (void)t;
    uint32_t elapsed = lv_tick_elaps(s_step_t0);
    switch (s_step) {
    case OB_CONNECT:
        if (portal_state() == PORTAL_SUBMITTED) {
            s_wifi_failed = false;
            show_step(OB_WIFI_TRY);
        }
        break;
    case OB_WIFI_TRY:
        if (s_connect_pending && elapsed > 1500) {
            s_connect_pending = false;
            char ssid[33], pass[65];
            if (identity_wifi_creds(ssid, sizeof(ssid), pass, sizeof(pass))) {
                net_wifi_connect(ssid, pass);
            }
        }
        if (s_celebrating) break; /* al acabar, wifi_connected() sigue */
        if (atomic_load(&s_wifi_up) && !s_connect_pending) {
            portal_stop();
            s_celebrating = true;
            ui_nowifi_celebrate(s_nw, false, celebrated);
        } else if (elapsed > WIFI_TIMEOUT_MS) {
            s_wifi_failed = true;
            net_wifi_disconnect();
            portal_rearm();
            show_step(OB_CONNECT); /* el portal sigue activo para reintentar */
        }
        break;
    case OB_SERVER:
        if (atomic_load(&s_mqtt_up) || elapsed > SERVER_TIMEOUT_MS) {
            show_step(OB_DONE);
        }
        break;
    default:
        break;
    }
}

/* Tras la celebracion: lo que antes pasaba nada mas conectar */
static void wifi_connected(void *arg)
{
    (void)arg;
    s_celebrating = false;
    s_nw = NULL; /* se borra con su pantalla */
    if (s_wifi_only) {
        /* nueva red OK: "¡Listo!" un segundo y reinicio limpio con la red guardada */
        lv_obj_t *scr = new_screen();
        title_label(scr, tr(STR_OB_DONE_TITLE));
        lv_timer_create(restart_timer_cb, 1200, NULL);
        s_step = OB_DONE;
        ESP_LOGI(TAG, "WiFi cambiada: reiniciando");
    } else if (!identity_has_creds()) {
        show_step(OB_DONE); /* sin credenciales MQTT no hay servidor al que ir */
    } else {
        show_step(OB_SERVER);
    }
}

/* Fin de la celebracion (desde su animacion): el cambio de pantalla, fuera de ella */
static void celebrated(void) { lv_async_call(wifi_connected, NULL); }

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    atomic_store(&s_wifi_up, ((const app_evt_wifi_t *)data)->connected);
}

static void on_mqtt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    atomic_store(&s_mqtt_up, ((const app_evt_mqtt_t *)data)->connected);
}

void ui_onboarding_start(void)
{
    app_events_subscribe(TWIN_EVT_WIFI, on_wifi, NULL);
    app_events_subscribe(TWIN_EVT_MQTT, on_mqtt, NULL);
    if (!lvgl_port_lock(2000)) {
        ESP_LOGE(TAG, "sin lock de LVGL");
        return;
    }
    show_step(OB_CONNECT);
    lv_timer_create(ob_timer_cb, 400, NULL);
    lvgl_port_unlock();
    ESP_LOGI(TAG, "asistente de primer arranque");
}

/* Llamada desde la tarea de LVGL (evento del icono WiFi de la pantalla principal). */
void ui_onboarding_start_wifi_only(void)
{
    static bool subscribed;
    if (!subscribed) {
        app_events_subscribe(TWIN_EVT_WIFI, on_wifi, NULL);
        subscribed = true;
    }
    s_wifi_only = true;
    s_wifi_failed = false;
    s_scr = NULL; /* no borrar la pantalla principal: new_screen() solo borra las suyas */
    show_step(OB_CONNECT);
    lv_timer_create(ob_timer_cb, 400, NULL);
    ESP_LOGI(TAG, "cambiar WiFi desde la pantalla principal");
}
