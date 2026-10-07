#include "captive_portal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "identity.h"
#include "portal_internal.h"

static const char *TAG = "portal";

#define AP_PASSWORD   "incutwin"
#define AP_MAX_SSIDS  15
#define PAGE_BUF_SIZE 4096
#define BODY_MAX      256

static esp_netif_t *s_ap_netif;
static httpd_handle_t s_httpd;
static volatile portal_state_t s_state = PORTAL_STOPPED;
static char s_ap_ssid[24];
static char *s_options; /* "<option>...</option>" de las redes escaneadas */
static char *s_page;    /* buffer de respuesta */

/* ------------------------------------------------------------------ utils */

static void url_decode(char *s)
{
    char *o = s;
    for (; *s; s++) {
        if (*s == '+') {
            *o++ = ' ';
        } else if (*s == '%' && s[1] && s[2]) {
            char hex[3] = { s[1], s[2], 0 };
            *o++ = (char)strtol(hex, NULL, 16);
            s += 2;
        } else {
            *o++ = *s;
        }
    }
    *o = '\0';
}

static void html_escape_into(char *out, size_t out_len, const char *in)
{
    size_t n = 0;
    for (; *in && n + 6 < out_len; in++) {
        switch (*in) {
        case '<': n += snprintf(out + n, out_len - n, "&lt;"); break;
        case '>': n += snprintf(out + n, out_len - n, "&gt;"); break;
        case '&': n += snprintf(out + n, out_len - n, "&amp;"); break;
        case '"': n += snprintf(out + n, out_len - n, "&quot;"); break;
        default: out[n++] = *in; out[n] = '\0'; break;
        }
    }
    out[n] = '\0';
}

/* ------------------------------------------------------------------- scan */

static void scan_networks(void)
{
    wifi_scan_config_t sc = { .show_hidden = false };
    esp_err_t err = esp_wifi_scan_start(&sc, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan: %s", esp_err_to_name(err));
        s_options[0] = '\0';
        return;
    }
    uint16_t n = 20;
    wifi_ap_record_t recs[20];
    esp_wifi_scan_get_ap_records(&n, recs);
    size_t len = 0;
    int added = 0;
    for (int i = 0; i < n && added < AP_MAX_SSIDS; i++) {
        const char *ssid = (const char *)recs[i].ssid;
        if (!ssid[0]) continue;
        bool dup = false;
        for (int j = 0; j < i && !dup; j++) dup = strcmp((const char *)recs[j].ssid, ssid) == 0;
        if (dup) continue;
        char esc[33 * 6];
        html_escape_into(esc, sizeof(esc), ssid);
        int w = snprintf(s_options + len, PAGE_BUF_SIZE / 2 - len, "<option>%s</option>", esc);
        if (w < 0 || len + w >= PAGE_BUF_SIZE / 2) break;
        len += w;
        added++;
    }
    ESP_LOGI(TAG, "scan: %d redes", added);
}

/* --------------------------------------------------------------- handlers */

static esp_err_t send_form(httpd_req_t *req, const char *error)
{
    portal_page_form(s_page, PAGE_BUF_SIZE, s_options, error);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req, s_page);
}

static esp_err_t root_get(httpd_req_t *req) { return send_form(req, NULL); }

static esp_err_t save_post(httpd_req_t *req)
{
    char body[BODY_MAX + 1];
    int total = req->content_len;
    if (total <= 0 || total > BODY_MAX) {
        return send_form(req, portal_text_fields_error());
    }
    int got = 0;
    while (got < total) {
        int r = httpd_req_recv(req, body + got, total - got);
        if (r <= 0) return ESP_FAIL;
        got += r;
    }
    body[got] = '\0';

    char ssid[33] = "", pass[65] = "";
    httpd_query_key_value(body, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(body, "pass", pass, sizeof(pass));
    url_decode(ssid);
    url_decode(pass);
    if (!ssid[0] || strlen(ssid) > 32 || strlen(pass) > 63) {
        return send_form(req, portal_text_fields_error());
    }
    identity_set_wifi_creds(ssid, pass);
    ESP_LOGI(TAG, "credenciales recibidas para '%s'", ssid);
    portal_page_done(s_page, PAGE_BUF_SIZE);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    esp_err_t err = httpd_resp_sendstr(req, s_page);
    s_state = PORTAL_SUBMITTED;
    return err;
}

/* Sondas de conectividad (generate_204, hotspot-detect.html, connecttest.txt...) */
static esp_err_t captive_redirect(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

/* -------------------------------------------------------------------- API */

esp_err_t portal_start(void)
{
    if (s_state != PORTAL_STOPPED) return ESP_OK;
    if (!s_options) s_options = calloc(1, PAGE_BUF_SIZE / 2);
    if (!s_page) s_page = malloc(PAGE_BUF_SIZE);
    ESP_RETURN_ON_FALSE(s_options && s_page, ESP_ERR_NO_MEM, TAG, "sin memoria");

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "IncuTwin-%02X%02X", mac[4], mac[5]);

    if (!s_ap_netif) s_ap_netif = esp_netif_create_default_wifi_ap();
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "apsta");
    scan_networks(); /* en STA, antes de levantar el AP */

    wifi_config_t ap = { 0 };
    strncpy((char *)ap.ap.ssid, s_ap_ssid, sizeof(ap.ap.ssid) - 1);
    ap.ap.ssid_len = strlen(s_ap_ssid);
    strncpy((char *)ap.ap.password, AP_PASSWORD, sizeof(ap.ap.password) - 1);
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap.ap.max_connection = 2;
    ap.ap.channel = 1;
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap), TAG, "ap config");

    ESP_RETURN_ON_ERROR(dns_responder_start(), TAG, "dns");

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.stack_size = 6144;
    cfg.core_id = 0;
    cfg.lru_purge_enable = true;
    ESP_RETURN_ON_ERROR(httpd_start(&s_httpd, &cfg), TAG, "httpd");
    const httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_get };
    const httpd_uri_t save = { .uri = "/save", .method = HTTP_POST, .handler = save_post };
    const httpd_uri_t any_get = { .uri = "/*", .method = HTTP_GET, .handler = captive_redirect };
    const httpd_uri_t any_post = { .uri = "/*", .method = HTTP_POST, .handler = captive_redirect };
    httpd_register_uri_handler(s_httpd, &root);
    httpd_register_uri_handler(s_httpd, &save);
    httpd_register_uri_handler(s_httpd, &any_get);
    httpd_register_uri_handler(s_httpd, &any_post);

    s_state = PORTAL_WAITING;
    ESP_LOGI(TAG, "portal activo: %s / %s en 192.168.4.1", s_ap_ssid, AP_PASSWORD);
    return ESP_OK;
}

void portal_stop(void)
{
    if (s_state == PORTAL_STOPPED) return;
    s_state = PORTAL_STOPPED;
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }
    dns_responder_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);
    free(s_options);
    free(s_page);
    s_options = NULL;
    s_page = NULL;
    ESP_LOGI(TAG, "portal parado");
}

portal_state_t portal_state(void) { return s_state; }

void portal_rearm(void)
{
    if (s_state == PORTAL_SUBMITTED) s_state = PORTAL_WAITING;
}

const char *portal_ap_ssid(void) { return s_ap_ssid; }
const char *portal_ap_password(void) { return AP_PASSWORD; }
