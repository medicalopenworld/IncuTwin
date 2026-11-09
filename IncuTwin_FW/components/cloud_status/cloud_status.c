#include "cloud_status.h"

#include <stdio.h>
#include <time.h>

#include "app_events.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hand_payload.h"
#include "identity.h"
#include "mqtt_link.h"
#include "net_wifi.h"
#include "sdkconfig.h"
#include "status_payload.h"
#include "twin_model.h"

static const char *TAG = "status";

static esp_timer_handle_t s_timer;

static void publish(const char *why)
{
    char topic[80], payload[STATUS_PAYLOAD_MAX + 1];
    char inc[TWIN_INCUBATOR_ID_LEN] = "";
    identity_incubator_id(inc);
    twin_snapshot_t s = twin_model_get();
    int64_t ts = net_wifi_time_valid() ? (int64_t)time(NULL) : 0;
    int n = status_payload_build(payload, sizeof(payload), esp_app_get_description()->version,
                                 s.link.rssi, inc, ts);
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
}

static void on_mqtt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    if (((const app_evt_mqtt_t *)data)->connected) {
        publish("conexion");
        esp_timer_stop(s_timer);
        esp_timer_start_periodic(s_timer, (uint64_t)CONFIG_INCUTWIN_STATUS_PERIOD_S * 1000000ULL);
    } else {
        esp_timer_stop(s_timer);
    }
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
    char inc[TWIN_INCUBATOR_ID_LEN] = "";
    identity_incubator_id(inc);
    if (inc[0] == '\0') return;
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
    const esp_timer_create_args_t t = { .callback = timer_cb, .name = "status_hourly" };
    ESP_ERROR_CHECK(esp_timer_create(&t, &s_timer));
    app_events_subscribe(TWIN_EVT_MQTT, on_mqtt, NULL);
    app_events_subscribe(TWIN_EVT_PAIRING_CHANGED, on_pairing, NULL);
    app_events_subscribe(TWIN_EVT_HAND_HOLD, on_hand_hold, NULL);
}
