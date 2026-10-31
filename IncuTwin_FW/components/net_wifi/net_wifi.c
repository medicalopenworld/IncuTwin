#include "net_wifi.h"

#include <string.h>
#include <time.h>

#include "app_events.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

static const char *TAG = "wifi";

#define BACKOFF_MIN_S 1
#define BACKOFF_MAX_S 30

static esp_netif_t *s_sta_netif;
static bool s_want_connect;    /* hay credenciales y queremos estar conectados */
static bool s_connected;
static int8_t s_rssi;
static char s_ip[16];
static uint32_t s_backoff_s = BACKOFF_MIN_S;
static esp_timer_handle_t s_retry_timer;
static esp_timer_handle_t s_rssi_timer;
static bool s_sntp_started;

static void post_wifi(void)
{
    app_evt_wifi_t ev = { .connected = s_connected, .rssi = s_rssi };
    strncpy(ev.ip, s_ip, sizeof(ev.ip) - 1);
    app_events_post(TWIN_EVT_WIFI, &ev, sizeof(ev));
}

static void retry_cb(void *arg)
{
    (void)arg;
    if (!s_want_connect) return;
    ESP_LOGI(TAG, "reintento de conexion");
    esp_wifi_connect();
}

static void schedule_retry(void)
{
    if (!s_want_connect) return;
    ESP_LOGI(TAG, "desconectada: reintento en %lu s", (unsigned long)s_backoff_s);
    esp_timer_stop(s_retry_timer);
    esp_timer_start_once(s_retry_timer, (uint64_t)s_backoff_s * 1000000ULL);
    s_backoff_s = s_backoff_s * 2 > BACKOFF_MAX_S ? BACKOFF_MAX_S : s_backoff_s * 2;
}

static void rssi_cb(void *arg)
{
    (void)arg;
    if (!s_connected) return;
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        s_rssi = ap.rssi;
        post_wifi();
    }
}

static void start_sntp(void)
{
    if (s_sntp_started) return;
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_INCUTWIN_SNTP_SERVER);
    if (esp_netif_sntp_init(&cfg) == ESP_OK) {
        s_sntp_started = true;
        ESP_LOGI(TAG, "sntp: %s", CONFIG_INCUTWIN_SNTP_SERVER);
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            if (s_want_connect) esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            bool was = s_connected;
            s_connected = false;
            s_ip[0] = '\0';
            esp_timer_stop(s_rssi_timer);
            if (was) post_wifi();
            schedule_retry();
            break;
        }
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));
        s_connected = true;
        s_backoff_s = BACKOFF_MIN_S;
        wifi_ap_record_t ap;
        s_rssi = esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : -100;
        ESP_LOGI(TAG, "ip %s rssi %d", s_ip, s_rssi);
        post_wifi();
        esp_timer_start_periodic(s_rssi_timer, 1000 * 1000);
        start_sntp();
    }
}

esp_err_t net_wifi_init(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    s_sta_netif = esp_netif_create_default_wifi_sta();
    ESP_RETURN_ON_FALSE(s_sta_netif, ESP_FAIL, TAG, "sta netif");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "storage");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL),
                        TAG, "wifi handler");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL),
                        TAG, "ip handler");

    const esp_timer_create_args_t retry = { .callback = retry_cb, .name = "wifi_retry" };
    ESP_RETURN_ON_ERROR(esp_timer_create(&retry, &s_retry_timer), TAG, "retry timer");
    const esp_timer_create_args_t rssi = { .callback = rssi_cb, .name = "wifi_rssi" };
    ESP_RETURN_ON_ERROR(esp_timer_create(&rssi, &s_rssi_timer), TAG, "rssi timer");

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "mode");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start");
    return ESP_OK;
}

esp_err_t net_wifi_connect(const char *ssid, const char *pass)
{
    wifi_config_t wc = { 0 };
    strncpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid) - 1);
    if (pass && pass[0]) {
        strncpy((char *)wc.sta.password, pass, sizeof(wc.sta.password) - 1);
        wc.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;
    } else {
        wc.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
    wc.sta.pmf_cfg.capable = true;
    wc.sta.pmf_cfg.required = false;
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wc), TAG, "config");
    s_want_connect = true;
    s_backoff_s = BACKOFF_MIN_S;
    ESP_LOGI(TAG, "conectando a '%s'", ssid);
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_CONN) err = ESP_OK; /* ya habia un intento en curso */
    return err;
}

void net_wifi_disconnect(void)
{
    s_want_connect = false;
    esp_timer_stop(s_retry_timer);
    esp_wifi_disconnect();
}

bool net_wifi_is_connected(void) { return s_connected; }
int8_t net_wifi_rssi(void) { return s_rssi; }

bool net_wifi_time_valid(void)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    return tm.tm_year + 1900 >= 2025;
}
