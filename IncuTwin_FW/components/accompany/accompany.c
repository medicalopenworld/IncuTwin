#include "accompany.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "acc_logic.h"
#include "app_events.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "identity.h"
#include "net_wifi.h"
#include "sound.h"
#include "storage.h"
#include "twin_logic.h"
#include "twin_model.h"

static const char *TAG = "acc";

#define TZ_SPAIN       "CET-1CEST,M3.5.0,M10.5.0/3"
#define PICK_WAIT_S    5 /* tras conectar: dejar llegar todos los retenidos antes de elegir */
#define KEY_MODE       "acc_mode"
#define KEY_STATE      "state"

typedef struct {
    uint16_t len;
    char json[APP_EVT_PAYLOAD_MAX + 1];
} stored_t;

static SemaphoreHandle_t s_mutex;
static bool s_enabled;
static bool s_started;
static acc_t s_acc;
static stored_t *s_store; /* ACC_MAX_INC entradas en PSRAM, mismo indice que s_acc.seen */

#define LOCK()   xSemaphoreTake(s_mutex, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_mutex)

static uint32_t now_s(void) { return (uint32_t)(esp_timer_get_time() / 1000000); }

/* Dias desde 1970-01-01 de una fecha civil (algoritmo de H. Hinnant). */
static int32_t days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int32_t)doe - 719468;
}

/* Dia y hora locales (España); ACC_NO_DAY / -1 sin hora SNTP. */
static void local_now(int32_t *day, int *hour)
{
    *day = ACC_NO_DAY;
    *hour = -1;
    if (!net_wifi_time_valid()) return;
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    *day = days_from_civil(tm.tm_year + 1900, (unsigned)tm.tm_mon + 1, (unsigned)tm.tm_mday);
    *hour = tm.tm_hour;
}

/* ------------------------------------------------------------- con el mutex */

static void save_locked(void)
{
    if (!storage_set_blob(STORAGE_NS_ACC, KEY_STATE, &s_acc.p, sizeof(s_acc.p))) {
        ESP_LOGW(TAG, "no se pudo guardar el historial");
    }
}

static void publish_ui_locked(void)
{
    if (!s_enabled) {
        twin_model_set_acc(TWIN_ACC_OFF, false, 0);
        return;
    }
    int32_t day;
    int hour;
    local_now(&day, &hour);
    twin_model_set_acc(acc_phase(&s_acc), acc_remind_active(&s_acc, day, hour), acc_week_mask(&s_acc, day));
}

/* La IncuNest actual ha cambiado: el gemelo pasa a ella con su ultimo estado guardado. */
static void apply_current_locked(void)
{
    const char *cur = acc_current(&s_acc);
    ESP_LOGI(TAG, "acompañando a %s", cur[0] ? cur : "(ninguna: buscando)");
    twin_model_set_pairing(cur);
    app_evt_pairing_t ev = { 0 };
    strncpy(ev.incubator_id, cur, TWIN_INCUBATOR_ID_LEN - 1);
    app_events_post(TWIN_EVT_PAIRING_CHANGED, &ev, sizeof(ev)); /* status; mqtt_link lo ignora */
    if (!cur[0] || !s_store) return;
    int i = acc_seen_index(&s_acc, cur);
    if (i >= 0 && s_store[i].len) twin_model_ingest(s_store[i].json, s_store[i].len);
}

static void handle_locked(uint32_t r)
{
    if (r & ACC_R_SWITCH) apply_current_locked();
    if (r & ACC_R_SAVE) save_locked();
    if (r & ACC_R_REMIND) {
        ESP_LOGI(TAG, "recordatorio: hoy aun no le has dado la mano");
        sound_request(SOUND_BABY); /* en horario de silencio lo descarta sound */
    }
    if (r & (ACC_R_PHASE | ACC_R_SWITCH | ACC_R_REMIND)) {
        ESP_LOGI(TAG, "fase %d, actual '%s'", acc_phase(&s_acc), acc_current(&s_acc));
    }
    publish_ui_locked();
}

/* ------------------------------------------------------------------ eventos */

static void on_mqtt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    if (!((const app_evt_mqtt_t *)data)->connected) return;
    LOCK();
    acc_hold_pick(&s_acc, now_s() + PICK_WAIT_S);
    UNLOCK();
}

static void on_input(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    if (!s_enabled) return;
    app_evt_acc_input_t in = *(const app_evt_acc_input_t *)data;
    int32_t day;
    int hour;
    local_now(&day, &hour);
    LOCK();
    uint32_t r = 0;
    if (in == APP_ACC_GESTURE) {
        r = acc_gesture(&s_acc, day);
        if (r) ESP_LOGI(TAG, "gesto completado: hoy cuenta como dia acompañado");
    } else if (in == APP_ACC_TOUCH) {
        r = acc_touch(&s_acc, now_s());
    }
    handle_locked(r);
    UNLOCK();
}

/* -------------------------------------------------------------------- API */

void accompany_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    setenv("TZ", TZ_SPAIN, 1);
    tzset();
    uint8_t mode = 0;
    s_enabled = storage_get_u8(STORAGE_NS_MQTT, KEY_MODE, &mode) && mode == 1;
    uint32_t seed = acc_hash(identity_mqtt_user());
    acc_persist_t p;
    if (storage_get_blob(STORAGE_NS_ACC, KEY_STATE, &p, sizeof(p))) {
        acc_restore(&s_acc, &p, seed, now_s());
    } else {
        acc_init(&s_acc, seed);
    }
    /* no elegir hasta conectar con el broker (on_mqtt pone la espera de verdad); el reloj se
     * compara con desbordamiento, asi que "nunca" es media vuelta por delante, no 0xFFFFFFFF */
    acc_hold_pick(&s_acc, now_s() + 0x70000000u);
    s_store = heap_caps_calloc(ACC_MAX_INC, sizeof(stored_t), MALLOC_CAP_SPIRAM);
    if (!s_store) ESP_LOGE(TAG, "sin PSRAM para los estados de las IncuNest");
    ESP_LOGI(TAG, "modo acompañar %s (fase %d, actual '%s', %u IncuNest en el historial)",
             s_enabled ? "ENCENDIDO" : "apagado", acc_phase(&s_acc), acc_current(&s_acc),
             (unsigned)s_acc.p.n_hist);
}

void accompany_start(void)
{
    app_events_subscribe(TWIN_EVT_MQTT, on_mqtt, NULL);
    app_events_subscribe(TWIN_EVT_ACC_INPUT, on_input, NULL);
    LOCK();
    s_started = true;
    if (s_enabled) publish_ui_locked(); /* el gemelo ya arranco con la actual; el estado llegara
                                         * con los retenidos */
    UNLOCK();
}

bool accompany_enabled(void) { return s_enabled; }

void accompany_current(char out[TWIN_INCUBATOR_ID_LEN])
{
    LOCK();
    strncpy(out, acc_current(&s_acc), TWIN_INCUBATOR_ID_LEN - 1);
    out[TWIN_INCUBATOR_ID_LEN - 1] = '\0';
    UNLOCK();
}

void accompany_set_enabled(bool on)
{
    LOCK();
    if (on == s_enabled) {
        UNLOCK();
        return;
    }
    s_enabled = on;
    if (on) {
        storage_set_u8(STORAGE_NS_MQTT, KEY_MODE, 1);
        /* empieza buscando, con el historial de antes */
        acc_persist_t p = s_acc.p;
        p.cur[0] = '\0';
        p.phase = TWIN_ACC_SEARCHING;
        p.engaged = 0;
        acc_restore(&s_acc, &p, s_acc.seed, now_s());
        acc_hold_pick(&s_acc, now_s() + PICK_WAIT_S);
        save_locked();
        twin_model_set_pairing("");
        ESP_LOGI(TAG, "modo acompañar encendido");
    } else {
        storage_erase_key(STORAGE_NS_MQTT, KEY_MODE);
        s_acc.n_seen = 0;
        sound_set_quiet(false);
        ESP_LOGI(TAG, "modo acompañar apagado");
    }
    publish_ui_locked();
    UNLOCK();
}

void accompany_ingest(const char *json, size_t len)
{
    if (!s_enabled || !s_started) return;
    twin_msg_t m;
    twin_parse_result_t pr = twin_parse(json, len, "", &m);
    if (pr != TWIN_PARSE_OK) {
        ESP_LOGW(TAG, "estado descartado: %s", twin_parse_result_str(pr));
        return;
    }
    bool present = m.state == TWIN_MSG_STATE_BABY &&
                   (!m.has_baby || m.baby == TWIN_BABY_IN || m.baby == TWIN_BABY_PARENTS);
    bool home = m.has_baby && m.baby == TWIN_BABY_HOME;
    LOCK();
    uint32_t r = acc_seen_update(&s_acc, m.incubator_id, present, home, now_s());
    int i = acc_seen_index(&s_acc, m.incubator_id);
    if (i >= 0 && s_store && len <= APP_EVT_PAYLOAD_MAX) {
        memcpy(s_store[i].json, json, len);
        s_store[i].json[len] = '\0';
        s_store[i].len = (uint16_t)len;
    }
    if (!(r & ACC_R_SWITCH) && (r & ACC_R_FORWARD)) twin_model_ingest(json, len);
    handle_locked(r); /* con SWITCH aplica el guardado, que es este si es la nueva actual */
    UNLOCK();
}

void accompany_tick_1s(void)
{
    if (!s_enabled || !s_started) return;
    int32_t day;
    int hour;
    local_now(&day, &hour);
    twin_snapshot_t s = twin_model_get();
    bool shown = !s.demo && (s.show_baby || s.parents_view);
    sound_set_quiet(acc_quiet(hour));
    LOCK();
    handle_locked(acc_tick(&s_acc, now_s(), day, hour, shown));
    UNLOCK();
}
