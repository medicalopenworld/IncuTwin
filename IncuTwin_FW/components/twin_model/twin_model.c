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
#include "twin_notify.h"

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
static uint8_t s_demo_skin; /* tono que se esta probando en la demo */

static twin_acc_t s_acc;     /* modo Acompañar */
static bool s_acc_remind;
static uint8_t s_acc_week;
static uint8_t s_acc_super;
static uint8_t s_acc_rest;

static twin_notice_base_t s_base;      /* contra lo que se comparan los avisos reales */
static twin_notice_base_t s_demo_base; /* idem en la demo */

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
        s.acc = s_acc;
        s.acc_remind = s_acc_remind;
        s.acc_week = s_acc_week;
        s.acc_super = s_acc_super;
        s.acc_rest = s_acc_rest;
    }
    twin_derive(&s);
    return s;
}

static void publish_locked(void)
{
    twin_snapshot_t s = build_snapshot();
    app_events_post(TWIN_EVT_STATE_CHANGED, &s, sizeof(s));
}

/* Fuera del mutex. Sin incubadora (demo) el id va vacio. */
static void post_notice(uint32_t mask, bool demo, const char *inc)
{
    if (mask == 0) return;
    app_evt_notice_t ev = { .mask = mask, .demo = demo };
    if (inc) strncpy(ev.inc, inc, TWIN_INCUBATOR_ID_LEN - 1);
    ESP_LOGI(TAG, "aviso 0x%02lx%s", (unsigned long)mask, demo ? " (demo)" : "");
    app_events_post(TWIN_EVT_NOTICE, &ev, sizeof(ev));
}

static void persist_locked(void)
{
#if CONFIG_INCUTWIN_SIM
    return; /* el build sim no escribe en el espacio del producto (twin/seq, twin/baby) */
#endif
    storage_set_u32(STORAGE_NS_TWIN, "seq", s_last_seq);
    storage_set_u8(STORAGE_NS_TWIN, "baby", (uint8_t)s_inc.baby);
}

/* Persistencia al desemparejar/emparejar de nuevo: son cachés, se borran las claves en vez de escribir
 * "ninguno"; el primer mensaje tras el siguiente arranque arma la base sin avisar. */
static void erase_persisted_locked(void)
{
#if !CONFIG_INCUTWIN_SIM
    storage_erase_key(STORAGE_NS_TWIN, "seq");
    storage_erase_key(STORAGE_NS_TWIN, "baby");
#endif
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
#if CONFIG_INCUTWIN_SIM
    /* el sim no usa twin/seq ni twin/baby: borra los que dejo un sim anterior (son cachés) */
    storage_erase_key(STORAGE_NS_TWIN, "seq");
    storage_erase_key(STORAGE_NS_TWIN, "baby");
#endif
    twin_incubator_reset(&s_inc, default_skin);
    memset(&s_link, 0, sizeof(s_link));
    s_link.has_creds = has_creds;
    if (incubator_id && incubator_id[0]) {
        strncpy(s_link.incubator_id, incubator_id, TWIN_INCUBATOR_ID_LEN - 1);
        s_link.paired = true;
        uint8_t b = 0;
#if CONFIG_INCUTWIN_SIM
        bool has_baby = false; /* el sim no lee twin/seq ni twin/baby */
#else
        storage_get_u32(STORAGE_NS_TWIN, "seq", &s_last_seq);
        bool has_baby = storage_get_u8(STORAGE_NS_TWIN, "baby", &b) && b <= TWIN_BABY_HOME;
#endif
        if (has_baby) {
            s_inc.baby = (twin_baby_t)b; /* para no repetir avisos tras reiniciar */
        }
        twin_notice_base_boot(&s_base, has_baby, (twin_baby_t)b);
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
            s_base.valid = false; /* otra incubadora: el primer estado no avisa */
            erase_persisted_locked();
            ESP_LOGI(TAG, "emparejado con %s", s_link.incubator_id);
            publish_locked();
        }
    } else if (had) {
        s_link.paired = false;
        s_link.state_rx = false;
        s_link.incubator_id[0] = '\0';
        s_last_seq = 0;
        twin_incubator_reset(&s_inc, s_default_skin);
        s_base.valid = false;
        erase_persisted_locked();
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
    twin_apply_result_t ar = twin_apply(&s_inc, &s_last_seq, &m);
    s_link.state_rx = true;
    if (ar.persist) persist_locked();
    ESP_LOGI(TAG, "estado %s seq=%lu %s baby=%d thermo=%d photo=%d bpm=%u ev=%s",
             ar.is_new ? "nuevo" : "refresco", (unsigned long)(m.has_seq ? m.seq : 0),
             s_inc.online ? "online" : "OFFLINE", s_inc.baby, s_inc.thermo, s_inc.photo,
             s_inc.bpm, m.last_event);
    uint32_t notice = 0;
    char inc_id[TWIN_INCUBATOR_ID_LEN];
    strncpy(inc_id, s_link.incubator_id, sizeof(inc_id));
    inc_id[sizeof(inc_id) - 1] = '\0';
    if (ar.is_new) {
        /* avisos reales aunque la demo este en pantalla: la base no se congela */
        notice = twin_notices_update(&s_base, &s_inc);
    }
    publish_locked();
    UNLOCK();
    post_notice(notice, false, inc_id);
}

/* ------------------------------------------------------------------- demo */

void twin_model_demo_enter(void)
{
    LOCK();
    s_demo = true;
    s_demo_linked = false;
    s_demo_skin = s_inc.skin; /* arranca con el tono real; se puede probar otro en ajustes */
    twin_incubator_reset(&s_demo_inc, s_demo_skin);
    s_demo_base.valid = false;
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
    s_demo_inc = *inc;
    s_demo_inc.skin = s_demo_skin; /* el escenario no cambia el tono que se esta probando */
    s_demo_linked = linked;
    uint32_t notice = twin_notices_update(&s_demo_base, &s_demo_inc);
    publish_locked();
    UNLOCK();
    post_notice(notice, true, "");
}

void twin_model_demo_set_skin(uint8_t skin)
{
    if (skin >= TWIN_SKIN_COUNT) return;
    LOCK();
    if (!s_demo || s_demo_skin == skin) {
        UNLOCK();
        return;
    }
    s_demo_skin = skin;
    s_demo_inc.skin = skin;
    publish_locked();
    UNLOCK();
}

void twin_model_set_acc(twin_acc_t acc, bool remind, uint8_t week, uint8_t super, uint8_t rest)
{
    LOCK();
    if (acc != s_acc || remind != s_acc_remind || week != s_acc_week || super != s_acc_super ||
        rest != s_acc_rest) {
        s_acc_rest = rest;
        s_acc = acc;
        s_acc_remind = remind;
        s_acc_week = week;
        s_acc_super = super;
        publish_locked();
    }
    UNLOCK();
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
    s_base.valid = false; /* el primer estado SIM tras arrancar no avisa */
    s_last_seq = 0;
    publish_locked();
    UNLOCK();
}

void twin_model_sim_apply(const twin_incubator_t *inc, bool linked)
{
    LOCK();
    s_inc = *inc;
    s_link.paired = linked;
    s_link.state_rx = linked;
    uint32_t notice = twin_notices_update(&s_base, &s_inc);
    publish_locked();
    UNLOCK();
    post_notice(notice, false, "SIM");
}
#endif
