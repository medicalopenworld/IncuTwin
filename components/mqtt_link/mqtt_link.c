#include "mqtt_link.h"

#include <stdio.h>
#include <string.h>

#include "app_events.h"
#include "backoff.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "identity.h"
#include "mqtt_client.h"
#include "sdkconfig.h"
#include "twin_types.h"

static const char *TAG = "mqtt";

#define LWT_PAYLOAD "{\"online\":false}"

static esp_mqtt_client_handle_t s_client;
static bool s_started;      /* esp_mqtt_client_start hecho */
static bool s_connected;
static bool s_wifi_up;
static char s_client_id[48];
static char s_status_topic[80];
static char s_cmd_topic[96];
static char s_incubator_id[TWIN_INCUBATOR_ID_LEN];
static char s_state_topic[64];
static backoff_t s_backoff;
static esp_timer_handle_t s_retry_timer;
/* msg_id de las dos ultimas suscripciones a incubators/<id>/state (al conectar y al llegar el
 * cmd/pair retenido se suscribe dos veces seguidas): para reconocer su SUBACK */
static int s_state_sub_msg_id[2] = { -1, -1 };

static bool is_state_sub(int msg_id)
{
    return msg_id == s_state_sub_msg_id[0] || msg_id == s_state_sub_msg_id[1];
}

/* ----------------------------------------------------------------- helpers */

static void post_connected(bool connected)
{
    app_evt_mqtt_t ev = { .connected = connected };
    app_events_post(TWIN_EVT_MQTT, &ev, sizeof(ev));
}

static void schedule_retry(void)
{
    if (!s_wifi_up) return;
    uint32_t nominal = backoff_next_nominal(&s_backoff);
    uint32_t ms = backoff_apply_jitter_ms(nominal, esp_random());
    ESP_LOGI(TAG, "reintento en %lu ms", (unsigned long)ms);
    esp_timer_stop(s_retry_timer);
    esp_timer_start_once(s_retry_timer, (uint64_t)ms * 1000ULL);
}

static void retry_cb(void *arg)
{
    (void)arg;
    if (!s_wifi_up || s_connected) return;
    if (!s_started) {
        if (esp_mqtt_client_start(s_client) == ESP_OK) s_started = true;
    } else {
        esp_mqtt_client_reconnect(s_client);
    }
}

static void subscribe_state_topic(void)
{
    if (!s_connected || !s_incubator_id[0]) return;
    snprintf(s_state_topic, sizeof(s_state_topic), "incubators/%s/state", s_incubator_id);
    s_state_sub_msg_id[1] = s_state_sub_msg_id[0];
    s_state_sub_msg_id[0] = esp_mqtt_client_subscribe(s_client, s_state_topic, 1);
    ESP_LOGI(TAG, "suscrito a %s", s_state_topic);
}

/* -------------------------------------------------------------- mqtt events */

static void on_mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    esp_mqtt_event_handle_t ev = data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        backoff_reset(&s_backoff);
        esp_timer_stop(s_retry_timer);
        ESP_LOGI(TAG, "connected");
        post_connected(true); /* cloud_status publica status; ota marca la app valida */
        esp_mqtt_client_subscribe(s_client, s_cmd_topic, 1);
        esp_mqtt_client_subscribe(s_client, "incutwin/all/cmd/#", 1);
        subscribe_state_topic();
        break;
    case MQTT_EVENT_DISCONNECTED:
        if (s_connected) {
            s_connected = false;
            ESP_LOGW(TAG, "desconectado");
            post_connected(false);
        }
        schedule_retry();
        break;
    case MQTT_EVENT_SUBSCRIBED:
        if (ev->error_handle && ev->error_handle->error_type == MQTT_ERROR_TYPE_SUBSCRIBE_FAILED) {
            if (is_state_sub(ev->msg_id) && s_incubator_id[0]) {
                /* el broker ya no nos deja ver esa incubadora: nos han desemparejado
                 * mientras estabamos desconectados (spec pairing) */
                ESP_LOGW(TAG, "suscripcion a %s rechazada: desemparejado por el broker", s_state_topic);
                app_evt_pairing_t pev = { 0 };
                strncpy(pev.incubator_id, s_incubator_id, TWIN_INCUBATOR_ID_LEN - 1);
                app_events_post(TWIN_EVT_STATE_SUB_REJECTED, &pev, sizeof(pev));
            } else {
                ESP_LOGW(TAG, "suscripcion rechazada por el broker (msg %d): falta la ACL", ev->msg_id);
            }
        }
        break;
    case MQTT_EVENT_DATA: {
        if (ev->total_data_len != ev->data_len || ev->current_data_offset != 0) {
            ESP_LOGW(TAG, "mensaje fragmentado de %d B descartado", ev->total_data_len);
            break;
        }
        if (ev->topic_len >= APP_EVT_TOPIC_MAX || ev->data_len > APP_EVT_PAYLOAD_MAX) {
            ESP_LOGW(TAG, "mensaje descartado: topic %d B / payload %d B", ev->topic_len, ev->data_len);
            break;
        }
        static app_evt_mqtt_msg_t msg; /* ~1,1 KB: no en la pila de la tarea MQTT */
        memcpy(msg.topic, ev->topic, ev->topic_len);
        msg.topic[ev->topic_len] = '\0';
        memcpy(msg.payload, ev->data, ev->data_len);
        msg.payload[ev->data_len] = '\0';
        msg.len = (uint16_t)ev->data_len;
        app_events_post(TWIN_EVT_MQTT_MSG, &msg, sizeof(msg));
        break;
    }
    case MQTT_EVENT_ERROR:
        if (ev->error_handle) {
            if (ev->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGW(TAG, "transporte/TLS: esp_tls=0x%x cert=0x%x errno=%d",
                         ev->error_handle->esp_tls_last_esp_err,
                         ev->error_handle->esp_tls_cert_verify_flags,
                         ev->error_handle->esp_transport_sock_errno);
            } else if (ev->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGW(TAG, "CONNACK rechazado: codigo %d (5 = not authorized)",
                         ev->error_handle->connect_return_code);
            }
        }
        break;
    default:
        break;
    }
}

/* -------------------------------------------------------------- app events */

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    bool up = ((const app_evt_wifi_t *)data)->connected;
    if (up == s_wifi_up) return;
    s_wifi_up = up;
    if (up) {
        backoff_reset(&s_backoff);
        esp_timer_stop(s_retry_timer);
        esp_timer_start_once(s_retry_timer, 500 * 1000); /* conectar en cuanto haya IP */
    } else {
        esp_timer_stop(s_retry_timer); /* sin WiFi no se cuenta ni se intenta */
    }
}

static void on_pairing(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    mqtt_link_set_incubator(((const app_evt_pairing_t *)data)->incubator_id);
}

/* -------------------------------------------------------------------- API */

esp_err_t mqtt_link_start(void)
{
    if (!identity_has_creds()) {
        ESP_LOGW(TAG, "sin credenciales: no se conecta al broker");
        return ESP_ERR_INVALID_STATE;
    }
    char pass[72];
    if (!identity_mqtt_pass(pass, sizeof(pass))) return ESP_ERR_INVALID_STATE;
    strncpy(s_client_id, identity_mqtt_user(), sizeof(s_client_id) - 1);
    snprintf(s_status_topic, sizeof(s_status_topic), "incutwin/%s/status", s_client_id);
    snprintf(s_cmd_topic, sizeof(s_cmd_topic), "incutwin/%s/cmd/#", s_client_id);
    identity_incubator_id(s_incubator_id);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_INCUTWIN_MQTT_URI,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.client_id = s_client_id,
        .credentials.username = s_client_id,
        .credentials.authentication.password = pass,
        .session.keepalive = CONFIG_INCUTWIN_MQTT_KEEPALIVE_S,
        .session.disable_clean_session = false,
        .session.last_will = {
            .topic = s_status_topic,
            .msg = LWT_PAYLOAD,
            .msg_len = sizeof(LWT_PAYLOAD) - 1,
            .qos = 1,
            .retain = true,
        },
        .network.disable_auto_reconnect = true, /* el backoff lo dirige este modulo */
        .network.timeout_ms = 10000,
        .buffer.size = 2048,
        .task.priority = 5,
        .task.stack_size = 8192,
    };
    s_client = esp_mqtt_client_init(&cfg); /* copia las cadenas: la contrasena no queda aqui */
    memset(pass, 0, sizeof(pass));
    if (!s_client) return ESP_FAIL;
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, on_mqtt_event, NULL);

    backoff_init(&s_backoff, 1, CONFIG_INCUTWIN_BACKOFF_MAX_S);
    const esp_timer_create_args_t t = { .callback = retry_cb, .name = "mqtt_retry" };
    ESP_ERROR_CHECK(esp_timer_create(&t, &s_retry_timer));

    app_events_subscribe(TWIN_EVT_WIFI, on_wifi, NULL);
    app_events_subscribe(TWIN_EVT_PAIRING_CHANGED, on_pairing, NULL);
    ESP_LOGI(TAG, "cliente %s -> %s (lwt %s)", s_client_id, CONFIG_INCUTWIN_MQTT_URI, s_status_topic);
    return ESP_OK;
}

bool mqtt_link_connected(void) { return s_connected; }
const char *mqtt_link_client_id(void) { return s_client_id; }

void mqtt_link_set_incubator(const char *incubator_id)
{
    const char *new_id = incubator_id ? incubator_id : "";
    if (strcmp(new_id, s_incubator_id) == 0) {
        subscribe_state_topic(); /* asegurar la suscripcion */
        return;
    }
    if (s_connected && s_incubator_id[0]) {
        esp_mqtt_client_unsubscribe(s_client, s_state_topic);
        ESP_LOGI(TAG, "desuscrito de %s", s_state_topic);
    }
    strncpy(s_incubator_id, new_id, TWIN_INCUBATOR_ID_LEN - 1);
    s_incubator_id[TWIN_INCUBATOR_ID_LEN - 1] = '\0';
    subscribe_state_topic();
}

bool mqtt_link_publish(const char *topic, const char *payload, size_t len, int qos, bool retain)
{
    if (!s_connected) return false;
    int id = esp_mqtt_client_publish(s_client, topic, payload, (int)len, qos, retain ? 1 : 0);
    return id >= 0;
}
