#include "cloud_status.h"

#include <stdio.h>
#include <time.h>

#include "accompany.h"
#include "app_events.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "hand_payload.h"
#include "identity.h"
#include "metrics_payload.h"
#include "rewards.h"
#include "settings.h"
#include "telemetry.h"
#include "usage_stats.h"
#include "mqtt_link.h"
#include "net_wifi.h"
#include "sdkconfig.h"
#include "status_payload.h"
#include "twin_model.h"

static const char *TAG = "status";

static esp_timer_handle_t s_timer;
static esp_timer_handle_t s_metrics_timer; /* agrupa los cambios de recompensas (30 s) */
#define METRICS_DEBOUNCE_US (30ULL * 1000000ULL)

/* Uso y recompensas para ThingsBoard (spec device-metrics): QoS 1, no retenido. Lo de la demo no
 * se publica. */
static SemaphoreHandle_t s_metrics_mtx; /* publish_metrics corre en la tarea de esp_timer y en la de eventos */
static char s_metrics_payload[METRICS_PAYLOAD_MAX + 1]; /* estatico: la pila de esp_timer es de 3,5 KB */

static void publish_metrics_locked(const char *why);

static void publish_metrics(const char *why)
{
    xSemaphoreTake(s_metrics_mtx, portMAX_DELAY);
    publish_metrics_locked(why);
    xSemaphoreGive(s_metrics_mtx);
}

static void publish_metrics_locked(const char *why)
{
    char topic[80];
    char *const payload = s_metrics_payload;
    rw_state_t r;
    bool demo = false;
    rewards_copy(&r, &demo);
    if (demo) return;
    usage_stats_t us = usage_get();
    metrics_usage_t u = { .ts = net_wifi_time_valid() ? (int64_t)time(NULL) : 0, .on_s = us.on_s,
                          .conn_s = us.conn_s, .hand_s = us.hand_s, .hand_n = us.hand_n };
    static const char *const PHASES[] = { "off", "searching", "waiting", "with", "ending", "farewell" };
    twin_snapshot_t s = twin_model_get(); /* una sola foto: inc y phase coherentes con status */
    const char *inc = s.link.paired ? s.link.incubator_id : "";
    unsigned ph = (unsigned)s.acc;
    metrics_extra_t x = { .gen = telemetry_gen(), .boot_n = telemetry_boot(), .demo_n = telemetry_demo_count(),
                          .q = telemetry_pending(), .lost = telemetry_lost(),
                          .fw = esp_app_get_description()->version, .inc = inc,
                          .phase = PHASES[ph < sizeof(PHASES) / sizeof(PHASES[0]) ? ph : 0],
                          .lang = settings_lang(), .vol = settings_volume(), .bright = settings_brightness() };
    int n = metrics_payload_build(payload, sizeof(s_metrics_payload), &u, &r, &x);
    if (n < 0) return;
    snprintf(topic, sizeof(topic), "incutwin/%s/metrics", mqtt_link_client_id());
    if (mqtt_link_publish(topic, payload, (size_t)n, 1, false)) ESP_LOGI(TAG, "metrics (%s): %s", why, payload);

    /* el canje de la IncuNest con tu nombre, hasta que llegue una vez */
    if ((r.unlocked & RW_BIT(RW_ITEM_NAMED_INCUNEST)) && !r.redeem_sent) {
        char rp[REDEEM_PAYLOAD_MAX + 1];
        n = redeem_payload_build(rp, sizeof(rp), u.ts);
        snprintf(topic, sizeof(topic), "incutwin/%s/redeem", mqtt_link_client_id());
        if (n > 0 && mqtt_link_publish(topic, rp, (size_t)n, 1, false)) {
            ESP_LOGI(TAG, "canje: %s", rp);
            rewards_mark_redeem_sent();
        }
    }
}

static void metrics_timer_cb(void *arg)
{
    (void)arg;
    publish_metrics("recompensas");
}

static void on_rewards(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;
    esp_timer_stop(s_metrics_timer); /* reinicia la espera: un gesto y su sello van juntos */
    esp_timer_start_once(s_metrics_timer, METRICS_DEBOUNCE_US);
}

static void publish(const char *why)
{
    char topic[80], payload[STATUS_PAYLOAD_MAX + 1];
    twin_snapshot_t s = twin_model_get();
    /* la del gemelo: la emparejada o, en modo acompañar, la que se acompaña ahora */
    const char *inc = s.link.paired ? s.link.incubator_id : "";
    int64_t ts = net_wifi_time_valid() ? (int64_t)time(NULL) : 0;
    char name[SETTINGS_NAME_BYTES];
    settings_name(name, sizeof(name)); /* el de la IncuTwin para la IncuPray; "" = sin nombre */
    int n = status_payload_build_named(payload, sizeof(payload), esp_app_get_description()->version,
                                       s.link.rssi, inc, accompany_enabled(), ts, name);
    if (n < 0) return;
    snprintf(topic, sizeof(topic), "incutwin/%s/status", mqtt_link_client_id());
    if (mqtt_link_publish(topic, payload, (size_t)n, 1, true)) {
        ESP_LOGI(TAG, "status (%s): %s", why, payload);
    }
}

static void timer_cb(void *arg)
{
    (void)arg;
    publish("periodico");
    publish_metrics("periodico");
}

static void on_mqtt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    if (((const app_evt_mqtt_t *)data)->connected) {
        publish("conexion");
        publish_metrics("conexion");
        esp_timer_stop(s_timer);
        esp_timer_start_periodic(s_timer, (uint64_t)CONFIG_INCUTWIN_STATUS_PERIOD_S * 1000000ULL);
    } else {
        esp_timer_stop(s_timer);
    }
}

static void on_name(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;
    publish("nombre");
}

static void on_pairing(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;
    publish("emparejado");
}

/* "Coge mi mano" completado (spec hold-my-hand): un aviso, QoS 1, no retenido */
static void on_hand_hold(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;
    char topic[80], payload[HAND_PAYLOAD_MAX + 1];
    twin_snapshot_t s = twin_model_get();
    if (!s.link.paired || s.link.incubator_id[0] == '\0') return;
    const char *inc = s.link.incubator_id;
    int n = hand_payload_build(payload, sizeof(payload), inc, CONFIG_INCUTWIN_HAND_MIN_S);
    if (n < 0) return;
    snprintf(topic, sizeof(topic), "incutwin/%s/hand", mqtt_link_client_id());
    if (mqtt_link_publish(topic, payload, (size_t)n, 1, false)) {
        ESP_LOGI(TAG, "hand: %s", payload);
    } else {
        ESP_LOGW(TAG, "hand: sin sesion, aviso perdido");
    }
}

void cloud_status_start(void)
{
    s_metrics_mtx = xSemaphoreCreateMutex();
    configASSERT(s_metrics_mtx);
    const esp_timer_create_args_t t = { .callback = timer_cb, .name = "status_hourly" };
    ESP_ERROR_CHECK(esp_timer_create(&t, &s_timer));
    const esp_timer_create_args_t m = { .callback = metrics_timer_cb, .name = "metrics" };
    ESP_ERROR_CHECK(esp_timer_create(&m, &s_metrics_timer));
    app_events_subscribe(TWIN_EVT_REWARDS_CHANGED, on_rewards, NULL);
    app_events_subscribe(TWIN_EVT_MQTT, on_mqtt, NULL);
    app_events_subscribe(TWIN_EVT_PAIRING_CHANGED, on_pairing, NULL);
    app_events_subscribe(TWIN_EVT_NAME_CHANGED, on_name, NULL);
    app_events_subscribe(TWIN_EVT_HAND_HOLD, on_hand_hold, NULL);
}
