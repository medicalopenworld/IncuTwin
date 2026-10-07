#include "twin_model.h"

#include <string.h>

#include "app_events.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include "storage.h"
#include "twin_logic.h"

static const char *TAG = "twin";

static SemaphoreHandle_t s_mutex;
static twin_link_t s_link;
static twin_incubator_t s_inc;        /* estado real */
static uint32_t s_last_seq;
static int64_t s_broker_lost_since;   /* us; 0 = no perdido */
static uint8_t s_default_skin;

static bool s_demo;
static twin_incubator_t s_demo_inc;
static bool s_demo_linked;

#define LOCK()   xSemaphoreTake(s_mutex, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_mutex)

/* ------------------------------------------------------------- snapshot */

/* Con el mutex cogido. */
static twin_snapshot_t build_snapshot(void)
{
    twin_snapshot_t s = { 0 };
    if (s_demo) {
        s.link = s_link;
        s.link.wifi = true;
        s.link.has_creds = true;
        s.link.broker_once = true;
        s.link.broker_lost = false;
        s.link.paired = s_demo_linked;
        s.link.state_rx = s_demo_linked;
        s.inc = s_demo_inc;
        s.demo = true;
    } else {
        s.link = s_link;
        s.inc = s_inc;
    }
    twin_derive(&s);
    return s;
}

static twin_baby_t presented_baby_locked(void)
{
    twin_snapshot_t s = build_snapshot();
    return s.parents_view ? s.inc.baby : (s.show_baby ? TWIN_BABY_IN : TWIN_BABY_NONE);
}

static void publish_locked(void)
{
    twin_snapshot_t s = build_snapshot();
    app_events_post(TWIN_EVT_STATE_CHANGED, &s, sizeof(s));
}

static void post_transitions(uint32_t mask)
{
    if (mask & TWIN_TR_BABY_IN) {
        twin_transition_t t = TWIN_TR_BABY_IN;
        app_events_post(TWIN_EVT_TRANSITION, &t, sizeof(t));
    }
    if (mask & TWIN_TR_BABY_PARENTS) {
        twin_transition_t t = TWIN_TR_BABY_PARENTS;
        app_events_post(TWIN_EVT_TRANSITION, &t, sizeof(t));
    }
}

static void persist_locked(void)
{
    storage_set_u32(STORAGE_NS_TWIN, "seq", s_last_seq);
    storage_set_u8(STORAGE_NS_TWIN, "baby", (uint8_t)s_inc.baby);
}

/* ----------------------------------------------------------------- events */

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    const app_evt_wifi_t *ev = data;
    if (ev->connected == s_link.wifi && ev->connected) {
        twin_model_set_rssi(ev->rssi); /* muestreo periodico */
    } else {
        twin_model_set_wifi(ev->connected, ev->rssi, ev->ip);
    }
}

static void on_mqtt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    twin_model_set_broker(((const app_evt_mqtt_t *)data)->connected);
}

static void on_pairing(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    twin_model_set_pairing(((const app_evt_pairing_t *)data)->incubator_id);
}

/* ------------------------------------------------------------------- init */

void twin_model_init(uint8_t default_skin, bool has_creds, const char *incubator_id)
{
    s_mutex = xSemaphoreCreateMutex();
    app_events_subscribe(TWIN_EVT_WIFI, on_wifi, NULL);
    app_events_subscribe(TWIN_EVT_MQTT, on_mqtt, NULL);
    app_events_subscribe(TWIN_EVT_PAIRING_CHANGED, on_pairing, NULL);
    s_default_skin = default_skin;
    twin_incubator_reset(&s_inc, default_skin);
    memset(&s_link, 0, sizeof(s_link));
    s_link.has_creds = has_creds;
    if (incubator_id && incubator_id[0]) {
        strncpy(s_link.incubator_id, incubator_id, TWIN_INCUBATOR_ID_LEN - 1);
        s_link.paired = true;
        uint8_t b = 0;
        storage_get_u32(STORAGE_NS_TWIN, "seq", &s_last_seq);
        if (storage_get_u8(STORAGE_NS_TWIN, "baby", &b) && b <= TWIN_BABY_HOME) {
            s_inc.baby = (twin_baby_t)b; /* para no repetir melodias tras reiniciar */
        }
        ESP_LOGI(TAG, "emparejado con %s (seq %lu, baby %u)", s_link.incubator_id,
                 (unsigned long)s_last_seq, b);
    } else {
        ESP_LOGI(TAG, "sin incubadora emparejada");
    }
}

/* ------------------------------------------------------------------ enlace */

void twin_model_set_wifi(bool connected, int8_t rssi, const char *ip)
{
    LOCK();
    s_link.wifi = connected;
    s_link.rssi = rssi;
    if (connected && ip) {
        strncpy(s_link.ip, ip, sizeof(s_link.ip) - 1);
    } else {
        s_link.ip[0] = '\0';
    }
    publish_locked();
    UNLOCK();
}

void twin_model_set_rssi(int8_t rssi)
{
    LOCK();
    bool changed = twin_rssi_level(rssi) != twin_rssi_level(s_link.rssi);
    s_link.rssi = rssi;
    if (changed) publish_locked();
    UNLOCK();
}

void twin_model_set_broker(bool connected)
{
    LOCK();
    s_link.broker_connected = connected;
    if (connected) {
        s_link.broker_once = true;
        s_link.broker_lost = false;
        s_broker_lost_since = 0;
    } else if (s_link.broker_once && s_broker_lost_since == 0) {
        s_broker_lost_since = esp_timer_get_time();
    }
    publish_locked();
    UNLOCK();
}

void twin_model_tick_1s(void)
{
    LOCK();
    if (!s_link.broker_connected && s_broker_lost_since != 0 && !s_link.broker_lost) {
        int64_t lost_s = (esp_timer_get_time() - s_broker_lost_since) / 1000000;
        if (lost_s >= CONFIG_INCUTWIN_BROKER_LOST_S) {
            s_link.broker_lost = true;
            ESP_LOGW(TAG, "%lld s sin broker: aviso en pantalla", (long long)lost_s);
            publish_locked();
        }
    }
    UNLOCK();
}

void twin_model_set_pairing(const char *incubator_id)
{
    LOCK();
    bool had = s_link.paired;
    if (incubator_id && incubator_id[0]) {
        bool same = had && strcmp(s_link.incubator_id, incubator_id) == 0;
        if (!same) {
            strncpy(s_link.incubator_id, incubator_id, TWIN_INCUBATOR_ID_LEN - 1);
            s_link.incubator_id[TWIN_INCUBATOR_ID_LEN - 1] = '\0';
            s_link.paired = true;
            s_link.state_rx = false;
            s_last_seq = 0;
            twin_incubator_reset(&s_inc, s_default_skin);
            persist_locked();
            ESP_LOGI(TAG, "emparejado con %s", s_link.incubator_id);
            publish_locked();
        }
    } else if (had) {
        s_link.paired = false;
        s_link.state_rx = false;
        s_link.incubator_id[0] = '\0';
        s_last_seq = 0;
        twin_incubator_reset(&s_inc, s_default_skin);
        persist_locked();
        ESP_LOGI(TAG, "desemparejado");
        publish_locked();
    }
    UNLOCK();
}

/* ----------------------------------------------------------------- ingest */

void twin_model_ingest(const char *json, size_t len)
{
    twin_msg_t m;
    LOCK();
    const char *expected = s_link.paired ? s_link.incubator_id : "";
    twin_parse_result_t pr = twin_parse(json, len, expected, &m);
    if (pr != TWIN_PARSE_OK) {
        UNLOCK();
        ESP_LOGW(TAG, "estado descartado: %s", twin_parse_result_str(pr));
        return;
    }
    twin_baby_t before = presented_baby_locked();
    twin_apply_result_t ar = twin_apply(&s_inc, &s_last_seq, &m);
    s_link.state_rx = true;
    if (ar.persist) persist_locked();
    ESP_LOGI(TAG, "estado %s seq=%lu %s baby=%d thermo=%d photo=%d bpm=%u ev=%s",
             ar.is_new ? "nuevo" : "refresco", (unsigned long)(m.has_seq ? m.seq : 0),
             s_inc.online ? "online" : "OFFLINE", s_inc.baby, s_inc.thermo, s_inc.photo,
             s_inc.bpm, m.last_event);
    uint32_t tr = 0;
    if (ar.is_new && !s_demo) {
        /* las transiciones se evaluan sobre lo que se presenta: sin enlace no suena */
        twin_baby_t after = presented_baby_locked();
        tr = twin_transitions_between(before, after);
    }
    publish_locked();
    UNLOCK();
    post_transitions(tr);
}

/* ------------------------------------------------------------------- demo */

void twin_model_demo_enter(void)
{
    LOCK();
    s_demo = true;
    s_demo_linked = false;
    twin_incubator_reset(&s_demo_inc, s_inc.skin);
    publish_locked();
    UNLOCK();
}

void twin_model_demo_exit(void)
{
    LOCK();
    s_demo = false;
    publish_locked();
    UNLOCK();
}

bool twin_model_demo_active(void) { return s_demo; }

void twin_model_demo_apply(const twin_incubator_t *inc, bool linked)
{
    LOCK();
    if (!s_demo) {
        UNLOCK();
        return;
    }
    twin_baby_t before = presented_baby_locked();
    s_demo_inc = *inc;
    s_demo_inc.skin = s_inc.skin; /* el tono real no cambia en la demo */
    s_demo_linked = linked;
    twin_baby_t after = presented_baby_locked();
    uint32_t tr = twin_transitions_between(before, after);
    publish_locked();
    UNLOCK();
    post_transitions(tr);
}

twin_snapshot_t twin_model_get(void)
{
    LOCK();
    twin_snapshot_t s = build_snapshot();
    UNLOCK();
    return s;
}

#if CONFIG_INCUTWIN_SIM
void twin_model_sim_link(bool linked)
{
    LOCK();
    s_link.has_creds = true;
    s_link.broker_connected = linked;
    s_link.broker_once = true;
    s_link.broker_lost = false;
    strcpy(s_link.incubator_id, "SIM");
    s_link.paired = true;
    publish_locked();
    UNLOCK();
}

void twin_model_sim_apply(const twin_incubator_t *inc, bool linked)
{
    LOCK();
    twin_baby_t before = presented_baby_locked();
    s_inc = *inc;
    s_link.paired = linked;
    s_link.state_rx = linked;
    twin_baby_t after = presented_baby_locked();
    uint32_t tr = twin_transitions_between(before, after);
    publish_locked();
    UNLOCK();
    post_transitions(tr);
}
#endif
