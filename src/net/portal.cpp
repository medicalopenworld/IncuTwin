#include "portal.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app/identity.h"
#include "config.h"
#include "ui/i18n.h"

static DNSServer *s_dns = nullptr;
static WebServer *s_http = nullptr;
static volatile portal_state_t s_state = PORTAL_STOPPED;
static char s_ap_ssid[24] = {0};
static String s_scan_options; /* cached <option> list */
static TaskHandle_t s_task = nullptr;

/* --------------------------------------------------------------------- HTML */

static const char PAGE_HEAD[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>IncuTwin</title><style>"
    "body{font-family:sans-serif;background:#F4F3F0;color:#1E3E6E;"
    "max-width:420px;margin:0 auto;padding:16px}"
    "h1{color:#054E92;font-size:1.4em}"
    "label{display:block;margin-top:14px;font-weight:bold}"
    "input,select{width:100%;padding:12px;margin-top:4px;font-size:1em;"
    "border:2px solid #054E92;border-radius:10px;box-sizing:border-box}"
    "button{width:100%;margin-top:20px;padding:14px;font-size:1.1em;"
    "background:#054E92;color:#fff;border:0;border-radius:12px}"
    ".g{display:flex;gap:8px;margin-top:14px;align-items:flex-start}"
    ".g input{width:auto;margin-top:3px}.g span{font-weight:normal;"
    "font-size:.9em}.err{color:#E0524A;font-weight:bold}"
    "</style></head><body>";

static String page_form(const char *err) {
    bool es = (g_lang == 0);
    String h = PAGE_HEAD;
    h += "<h1>IncuTwin</h1><p>";
    h += es ? "Configura tu panel IncuTwin."
            : "Set up your IncuTwin panel.";
    h += "</p>";
    if (err && err[0]) h += String("<p class='err'>") + err + "</p>";
    h += "<form method='POST' action='/save'>";
    h += es ? "<label>Red WiFi de casa</label>" : "<label>Home WiFi network</label>";
    h += "<select name='ssid'>" + s_scan_options + "</select>";
    h += es ? "<label>Contraseña WiFi</label>" : "<label>WiFi password</label>";
    h += "<input type='password' name='pass'>";
    h += es ? "<label>Tu nombre</label>" : "<label>Your name</label>";
    h += "<input name='name' maxlength='32' required>";
    h += "<label>Email</label><input type='email' name='email' "
         "maxlength='64' required>";
    h += "<div class='g'><input type='checkbox' name='gdpr' required><span>";
    h += es ? "Acepto la <a href='https://medicalopenworld.org/privacy'>"
              "política de privacidad</a> y el tratamiento de mis datos "
              "(RGPD)."
            : "I accept the <a href='https://medicalopenworld.org/privacy'>"
              "privacy policy</a> and the processing of my data (GDPR).";
    h += "</span></div><button type='submit'>";
    h += es ? "Conectar" : "Connect";
    h += "</button></form></body></html>";
    return h;
}

static String page_done(void) {
    bool es = (g_lang == 0);
    String h = PAGE_HEAD;
    h += "<h1>IncuTwin</h1><p style='font-size:1.2em'>";
    h += es ? "¡Datos recibidos! Mira la pantalla del panel para continuar."
            : "Done! Look at the panel screen to continue.";
    h += "</p></body></html>";
    return h;
}

/* ----------------------------------------------------------------- handlers */

static void handle_root(void) { s_http->send(200, "text/html", page_form("")); }

static void handle_save(void) {
    String ssid = s_http->arg("ssid");
    String pass = s_http->arg("pass");
    String name = s_http->arg("name");
    String email = s_http->arg("email");
    bool gdpr = s_http->hasArg("gdpr");

    bool es = (g_lang == 0);
    if (ssid.length() == 0 || name.length() == 0 ||
        email.indexOf('@') < 0 || !gdpr) {
        s_http->send(200, "text/html",
                     page_form(es ? "Revisa los campos marcados."
                                  : "Please check the fields."));
        return;
    }
    prov_set_wifi(ssid.c_str(), pass.c_str());
    prov_set_user(name.c_str(), email.c_str(), gdpr);
    s_http->send(200, "text/html", page_done());
    s_state = PORTAL_SUBMITTED;
}

static void handle_captive(void) {
    /* redirect any probe (generate_204, hotspot-detect...) to the portal */
    s_http->sendHeader("Location", "http://192.168.4.1/", true);
    s_http->send(302, "text/plain", "");
}

/* --------------------------------------------------------------------- task */

static void portal_task(void *arg) {
    (void)arg;
    while (s_state == PORTAL_WAITING || s_state == PORTAL_SUBMITTED) {
        if (s_dns) s_dns->processNextRequest();
        if (s_http) s_http->handleClient();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    vTaskDelete(nullptr);
}

/* ---------------------------------------------------------------------- API */

void portal_start(void) {
    if (s_state == PORTAL_WAITING) return;

    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), AP_SSID_PREFIX "%02X%02X", mac[4],
             mac[5]);

    /* scan first (STA), then bring up AP+STA */
    WiFi.mode(WIFI_AP_STA);
    int n = WiFi.scanNetworks();
    s_scan_options = "";
    for (int i = 0; i < n && i < 15; i++) {
        String ss = WiFi.SSID(i);
        if (ss.length() == 0) continue;
        s_scan_options += "<option>" + ss + "</option>";
    }
    WiFi.scanDelete();

    WiFi.softAP(s_ap_ssid, AP_PASSWORD);
    delay(100);

    s_dns = new DNSServer();
    s_dns->start(53, "*", WiFi.softAPIP());

    s_http = new WebServer(80);
    s_http->on("/", handle_root);
    s_http->on("/save", HTTP_POST, handle_save);
    s_http->onNotFound(handle_captive);
    s_http->begin();

    s_state = PORTAL_WAITING;
    xTaskCreatePinnedToCore(portal_task, "portal", 8192, nullptr, 1, &s_task,
                            0);
}

void portal_stop(void) {
    s_state = PORTAL_STOPPED;
    vTaskDelay(pdMS_TO_TICKS(50));
    if (s_http) {
        s_http->stop();
        delete s_http;
        s_http = nullptr;
    }
    if (s_dns) {
        s_dns->stop();
        delete s_dns;
        s_dns = nullptr;
    }
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
}

portal_state_t portal_state(void) { return s_state; }
const char *portal_ap_ssid(void) { return s_ap_ssid; }
