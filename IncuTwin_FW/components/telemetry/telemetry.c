/* IncuTwin — telemetria de uso: pegamento entre productores, cola de flash y broker (ver telemetry.h).
 *
 * Tarea "telemetry" (duena unica de s_q, s_peek, s_batch y del estado del lote en vuelo):
 *   1. abre la cola de flash (130-190 ms) al empezar, no en app_main;
 *   2. en cada vuelta (aviso o 1 s): pasa los eventos de la cola RAM a la flash numerandolos,
 *      atiende los avisos de control (MQTT arriba/abajo, PUBACK, flush, demo), vence el lote en
 *      vuelo sin PUBACK y decide si toca enviar.
 * Los demas solo tocan s_evq / s_ctlq (colas FreeRTOS, sin esperar) y leen contadores.
 *
 * Lote en vuelo: como mucho uno. Se recuerda su msg_id y su seq mas alto; solo el PUBACK de ese
 * msg_id lo confirma (tel_queue_ack_through + NVS tel/acked). Los PUBACK llegan como aviso de
 * control por la misma tarea, asi que el id ya esta anotado cuando se procesan aunque el broker
 * conteste antes de que vuelva mqtt_link_publish_id().
 */
#include "telemetry.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_events.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mqtt_link.h"
#include "storage.h"
#include "tel_logic.h"
#include "tel_queue.h"

static const char *TAG = "tel";

#define KEY_GEN    STORAGE_TEL_GEN
#define KEY_BOOT   "boot_n"
#define KEY_ACKED  "acked"
#define KEY_FW     "fw"
#define KEY_DEMO_N "demo_n"

#define EVQ_LEN          32                    /* eventos en RAM esperando a la tarea */
#define CTLQ_LEN         8
#define TICK_MS          1000
#define SEND_AFTER_US    (60LL * 1000000LL)    /* 60 s tras el primer evento sin enviar */
#define PUBACK_WAIT_US   (30LL * 1000000LL)    /* sin PUBACK en 30 s: el lote no llego */
#define RETRY_US         (5LL * 1000000LL)     /* tras un publish fallido */
#define NET_OFF_MIN_S    600                   /* evento net si se estuvo >= 10 min sin broker */
#define EPOCH_2025       1735689600LL          /* antes de esto no hay hora (sin SNTP) */
#define TASK_STACK       4608
#define TASK_PRIO        2

typedef struct {   /* evento en la cola RAM: lo que se sabe al registrarlo */
    int64_t  t;
    uint32_t up;
    char     kind[TEL_KIND_MAX];
    char     d[TEL_D_MAX];
} msg_t;

typedef enum { CTL_MQTT_UP = 1, CTL_MQTT_DOWN, CTL_PUBACK, CTL_FLUSH, CTL_DEMO_ON } ctl_kind_t;

typedef struct {
    uint8_t kind;  /* ctl_kind_t */
    int     arg;   /* msg_id del PUBACK */
} ctl_t;

/* --- fijo tras telemetry_init (lo leen todos) */
static uint32_t s_gen, s_boot;
static QueueHandle_t s_evq, s_ctlq;
static SemaphoreHandle_t s_flush_sem;
static TaskHandle_t s_task;
static const esp_partition_t *s_part;

/* --- compartido: escribe una tarea, leen otras (32 bits alineados) */
static volatile bool s_demo;
static volatile uint32_t s_demo_n;
static volatile uint32_t s_drops;      /* perdidos fuera de tel_queue (RAM llena, flash) */
static volatile bool s_open;
static volatile uint32_t s_open_ms;
static volatile int s_inflight_id = -1;
static volatile uint32_t s_inflight_first, s_inflight_last;
static volatile int64_t s_inflight_us;
static volatile bool s_connected;
static volatile uint32_t s_acked;

/* --- solo la tarea */
static tel_queue_t s_q;
static tel_event_t *s_peek;            /* TEL_BATCH_N eventos (4 KB) */
static char *s_batch;                  /* TEL_BATCH_MAX + 1 */
static int64_t s_unsent_us;            /* cuando entro el primer evento sin enviar; 0 = ninguno */
static bool s_send_now;
static int64_t s_retry_us;
static int64_t s_offline_us;           /* desde cuando no hay broker (0 = desde el arranque) */
static bool s_flush_wait;
static uint32_t s_flush_seq;

static void drops_add(uint32_t n)
{
    __atomic_fetch_add(&s_drops, n, __ATOMIC_RELAXED);
}

/* ------------------------------------------------------------------ flash */

static bool fl_read(void *ctx, uint32_t off, void *buf, size_t len)
{
    return esp_partition_read(ctx, off, buf, len) == ESP_OK;
}

static bool fl_write(void *ctx, uint32_t off, const void *buf, size_t len)
{
    return esp_partition_write(ctx, off, buf, len) == ESP_OK;
}

static bool fl_erase(void *ctx, uint32_t off, size_t len)
{
    return esp_partition_erase_range(ctx, off, len) == ESP_OK;
}

/* ------------------------------------------------------------- validacion */

static bool kind_ok(const char *k)
{
    if (!k) return false;
    size_t n = 0;
    for (; k[n]; n++) {
        if (n >= TEL_KIND_MAX - 1) return false;
        if (!((k[n] >= 'a' && k[n] <= 'z') || k[n] == '_')) return false;
    }
    return n > 0;
}

/* Objeto JSON plausible: {...} de < TEL_D_MAX bytes, sin caracteres de control, comillas
 * cerradas y llaves/corchetes equilibrados (lo barato; no valida numeros ni comas). */
static bool d_ok(const char *s)
{
    size_t n = strnlen(s, TEL_D_MAX);
    if (n < 2 || n >= TEL_D_MAX || s[0] != '{' || s[n - 1] != '}') return false;
    int depth = 0;
    bool str = false, esc = false;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20) return false;
        if (str) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') str = false;
            continue;
        }
        if (c == '"') {
            str = true;
        } else if (c == '{' || c == '[') {
            if (++depth > 8) return false;
        } else if (c == '}' || c == ']') {
            if (--depth < 0) return false;
            if (depth == 0 && i != n - 1) return false; /* algo detras del objeto */
        }
    }
    return !str && depth == 0;
}

static int64_t epoch_or_0(void)
{
    time_t t = time(NULL);
    return (int64_t)t >= EPOCH_2025 ? (int64_t)t : 0;
}

/* ------------------------------------------------------------------ envio */

static void log_batch(int n, uint32_t first, uint32_t last, int len, int id,
                      uint32_t gen, uint32_t boot, const char *kinds)
{
    ESP_LOGI(TAG, "lote publicado: %d eventos, seq %" PRIu32 "..%" PRIu32 ", %d B, gen %" PRIu32 " b %" PRIu32
                  ", msg_id %d [%s]",
             n, first, last, len, gen, boot, id, kinds);
}

static void send_batch(int64_t now)
{
    int n = tel_queue_peek(&s_q, s_peek, TEL_BATCH_N);
    if (n <= 0) return;
    /* un lote, un (gen, arranque): los eventos de otro arranque van en el siguiente */
    int k = 1;
    while (k < n && s_peek[k].gen == s_peek[0].gen && s_peek[k].boot == s_peek[0].boot) k++;
    int used = 0;
    int len = tel_batch_build(s_batch, TEL_BATCH_MAX + 1, s_peek[0].gen, s_peek[0].boot, s_peek, k, &used);
    if (len < 0 || used <= 0) {
        /* no deberia pasar (un evento ocupa < 300 B): se quita para no atascar la cola */
        ESP_LOGW(TAG, "el evento %" PRIu32 " no cabe en un lote: se descarta", s_peek[0].seq);
        tel_queue_ack_through(&s_q, s_peek[0].seq);
        drops_add(1);
        return;
    }
    char topic[80];
    snprintf(topic, sizeof(topic), "incutwin/%s/ev", mqtt_link_client_id());
    int id = mqtt_link_publish_id(topic, s_batch, (size_t)len, 1, false);
    if (id <= 0) {
        ESP_LOGW(TAG, "lote no enviado (sin sesion): se reintenta");
        s_retry_us = now + RETRY_US;
        return;
    }
    s_inflight_first = s_peek[0].seq;
    s_inflight_last = s_peek[used - 1].seq;
    s_inflight_us = now;
    s_inflight_id = id;
    s_send_now = false;
    s_unsent_us = 0; /* lo que quede sale tras el PUBACK */

    char kinds[96];
    size_t p = 0;
    kinds[0] = '\0';
    for (int i = 0; i < used && p < sizeof(kinds) - 1; i++) {
        int w = snprintf(kinds + p, sizeof(kinds) - p, "%s%s", i ? "," : "", s_peek[i].kind);
        if (w < 0) break;
        p += (size_t)w;
    }
    log_batch(used, s_inflight_first, s_inflight_last, len, id, s_peek[0].gen,
              s_peek[0].boot, kinds);
}

static void maybe_send(void)
{
    if (!s_open || !s_connected || s_inflight_id >= 0) return;
    uint32_t pend = tel_queue_pending(&s_q);
    if (!pend) {
        s_send_now = false;
        s_unsent_us = 0;
        return;
    }
    int64_t now = esp_timer_get_time();
    if (now < s_retry_us) return;
    bool due = s_send_now || pend >= TEL_BATCH_N || (s_unsent_us && now - s_unsent_us >= SEND_AFTER_US);
    if (due) send_batch(now);
}

static void flush_done(void)
{
    if (!s_flush_wait) return;
    s_flush_wait = false;
    xSemaphoreGive(s_flush_sem);
}

/* --------------------------------------------------------- cola en flash */

static void push(const msg_t *m)
{
    if (!s_open) {
        drops_add(1);
        return;
    }
    tel_event_t e;
    memset(&e, 0, sizeof(e)); /* relleno a cero: lo que va a la flash es determinista */
    e.seq = tel_queue_next_seq(&s_q);
    e.t = m->t;
    e.up = m->up;
    memcpy(e.kind, m->kind, sizeof(e.kind));
    memcpy(e.d, m->d, sizeof(e.d));
    e.gen = s_gen;
    e.boot = s_boot;
    if (!tel_queue_push(&s_q, &e)) {
        ESP_LOGW(TAG, "no se pudo guardar el evento %" PRIu32 " (%s)", e.seq, e.kind);
        drops_add(1);
        return;
    }
    if (!s_unsent_us) s_unsent_us = esp_timer_get_time();
}

static void drain_events(void)
{
    msg_t m;
    while (xQueueReceive(s_evq, &m, 0) == pdTRUE) push(&m);
}

/* Evento de la propia tarea (net): detras de los que esperan en RAM. */
static void log_here(const char *kind, const char *d)
{
    msg_t m;
    memset(&m, 0, sizeof(m));
    m.t = epoch_or_0();
    m.up = (uint32_t)(esp_timer_get_time() / 1000000LL);
    strncpy(m.kind, kind, sizeof(m.kind) - 1);
    strncpy(m.d, d, sizeof(m.d) - 1);
    drain_events();
    push(&m);
}

/* -------------------------------------------------------------- control */

static void on_ctl(const ctl_t *c)
{
    int64_t now = esp_timer_get_time();
    switch (c->kind) {
    case CTL_MQTT_UP: {
        if (s_connected) break; /* repetido (telemetry_start y el evento a la vez) */
        s_connected = true;
        int64_t off_s = (now - s_offline_us) / 1000000LL;
        if (off_s >= NET_OFF_MIN_S) {
            char d[32];
            snprintf(d, sizeof(d), "{\"off_s\":%" PRId64 "}", off_s);
            log_here("net", d);
        }
        s_inflight_id = -1; /* un lote de la sesion anterior no cuenta: vuelve a salir */
        s_send_now = true;
        s_retry_us = 0;
        break;
    }
    case CTL_MQTT_DOWN:
        if (s_connected) s_offline_us = now;
        s_connected = false;
        if (s_inflight_id >= 0) {
            ESP_LOGW(TAG, "sesion caida con el lote %d en vuelo (seq %" PRIu32 "..%" PRIu32 "): se reenviara",
                     s_inflight_id, s_inflight_first, s_inflight_last);
            s_inflight_id = -1;
        }
        flush_done();
        break;
    case CTL_PUBACK:
        if (s_inflight_id < 0 || c->arg != s_inflight_id) {
            ESP_LOGD(TAG, "PUBACK %d ajeno: se ignora", c->arg);
            break;
        }
        tel_queue_ack_through(&s_q, s_inflight_last);
        s_acked = s_inflight_last;
        storage_set_u32(STORAGE_NS_TEL, KEY_ACKED, s_acked);
        ESP_LOGI(TAG, "PUBACK %d: confirmados hasta seq %" PRIu32 ", quedan %" PRIu32, c->arg, s_inflight_last,
                 tel_queue_pending(&s_q));
        s_inflight_id = -1;
        s_send_now = true; /* si quedan, el siguiente lote sale ya */
        if (s_flush_wait && (s_acked >= s_flush_seq || tel_queue_pending(&s_q) == 0)) flush_done();
        break;
    case CTL_FLUSH:
        drain_events();
        if (s_flush_wait) flush_done(); /* uno anterior sin acabar: no se apilan */
        s_flush_seq = tel_queue_next_seq(&s_q) - 1;
        if (!s_open || !s_connected || tel_queue_pending(&s_q) == 0 || s_acked >= s_flush_seq) {
            xSemaphoreGive(s_flush_sem); /* nada que esperar */
        } else {
            s_flush_wait = true;
            s_send_now = true;
        }
        break;
    case CTL_DEMO_ON:
        s_demo_n = s_demo_n + 1;
        storage_set_u32(STORAGE_NS_TEL, KEY_DEMO_N, s_demo_n);
        break;
    default:
        break;
    }
}

static void post_ctl(uint8_t kind, int arg)
{
    if (!s_ctlq) return;
    ctl_t c = { .kind = kind, .arg = arg };
    if (xQueueSend(s_ctlq, &c, 0) != pdTRUE) {
        /* PUBACK perdido: lo cubre la espera de 30 s; MQTT: el siguiente cambio lo corrige */
        ESP_LOGW(TAG, "cola de control llena: aviso %u perdido", kind);
        return;
    }
    if (s_task) xTaskNotifyGive(s_task);
}

static void on_mqtt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    post_ctl(((const app_evt_mqtt_t *)data)->connected ? CTL_MQTT_UP : CTL_MQTT_DOWN, 0);
}

static void on_puback(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    post_ctl(CTL_PUBACK, ((const app_evt_puback_t *)data)->msg_id);
}

/* ---------------------------------------------------------------- tarea */

static void tel_task(void *arg)
{
    (void)arg;
    int64_t t0 = esp_timer_get_time();
    if (s_part) {
        tel_flash_t f = {
            .read = fl_read, .write = fl_write, .erase = fl_erase, .size = (uint32_t)s_part->size,
            .ctx = (void *)s_part,
        };
        tel_queue_open(&s_q, &f, s_acked);
        s_open = true;
    }
    s_open_ms = (uint32_t)((esp_timer_get_time() - t0) / 1000);
    if (s_open) {
        ESP_LOGI(TAG, "cola abierta en %" PRIu32 " ms: %" PRIu32 " pendientes, next_seq %" PRIu32
                      ", acked %" PRIu32 ", gen %" PRIu32 " b %" PRIu32,
                 s_open_ms, tel_queue_pending(&s_q), tel_queue_next_seq(&s_q), s_acked, s_gen, s_boot);
        if (tel_queue_pending(&s_q)) s_unsent_us = esp_timer_get_time();
    }
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TICK_MS));
        drain_events();
        ctl_t c;
        while (xQueueReceive(s_ctlq, &c, 0) == pdTRUE) on_ctl(&c);
        if (s_inflight_id >= 0 && esp_timer_get_time() - s_inflight_us >= PUBACK_WAIT_US) {
            ESP_LOGW(TAG, "sin PUBACK del lote %d en 30 s (seq %" PRIu32 "..%" PRIu32 "): se reenvia",
                     s_inflight_id, s_inflight_first, s_inflight_last);
            s_inflight_id = -1;
            s_send_now = true;
        }
        maybe_send();
    }
}

/* -------------------------------------------------------------- arranque */

static const char *reset_reason_name(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON:   return "poweron";
    case ESP_RST_EXT:       return "ext";
    case ESP_RST_SW:        return "sw";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "int_wdt";
    case ESP_RST_TASK_WDT:  return "task_wdt";
    case ESP_RST_WDT:       return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_SDIO:      return "sdio";
    case ESP_RST_USB:       return "usb";
    case ESP_RST_JTAG:      return "jtag";
    case ESP_RST_EFUSE:     return "efuse";
    case ESP_RST_PWR_GLITCH: return "pwr_glitch";
    case ESP_RST_CPU_LOCKUP: return "cpu_lockup";
    default:                return "unknown";
    }
}

/* El firmware `from` acabo en una particion marcada invalida: se volvio atras (rollback). */
static bool rolled_back_from(const char *from)
{
    const esp_partition_t *bad = esp_ota_get_last_invalid_partition();
    esp_app_desc_t desc;
    if (!bad || esp_ota_get_partition_description(bad, &desc) != ESP_OK) return false;
    return strncmp(desc.version, from, sizeof(desc.version)) == 0;
}

static void *alloc_prefer_psram(size_t n)
{
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_malloc(n, MALLOC_CAP_8BIT);
}

void telemetry_init(void)
{
    if (s_task) return;
    uint32_t v = 0;
    s_gen = storage_get_u32(STORAGE_NS_TEL, KEY_GEN, &v) && v ? v : 1;
    if (!v) storage_set_u32(STORAGE_NS_TEL, KEY_GEN, s_gen);
    v = 0;
    storage_get_u32(STORAGE_NS_TEL, KEY_BOOT, &v);
    s_boot = v + 1;
    storage_set_u32(STORAGE_NS_TEL, KEY_BOOT, s_boot);
    v = 0;
    storage_get_u32(STORAGE_NS_TEL, KEY_ACKED, &v);
    s_acked = v;
    v = 0;
    storage_get_u32(STORAGE_NS_TEL, KEY_DEMO_N, &v);
    s_demo_n = v;

    s_peek = alloc_prefer_psram(TEL_BATCH_N * sizeof(tel_event_t));
    s_batch = alloc_prefer_psram(TEL_BATCH_MAX + 1);
    s_evq = xQueueCreate(EVQ_LEN, sizeof(msg_t));
    s_ctlq = xQueueCreate(CTLQ_LEN, sizeof(ctl_t));
    s_flush_sem = xSemaphoreCreateBinary();
    if (!s_peek || !s_batch || !s_evq || !s_ctlq || !s_flush_sem) {
        ESP_LOGE(TAG, "sin memoria: telemetria apagada");
        if (s_evq) vQueueDelete(s_evq);
        s_evq = NULL;
        return;
    }
    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "storage");
    if (!s_part) ESP_LOGE(TAG, "sin particion storage: los eventos no se guardan");

    /* boot y, si cambio el firmware, ota: esperan en la cola RAM a que la tarea abra la flash */
    const char *fw = esp_app_get_description()->version;
    char d[TEL_D_MAX];
    snprintf(d, sizeof(d), "{\"reason\":\"%s\",\"fw\":\"%s\"}", reset_reason_name(esp_reset_reason()), fw);
    telemetry_log("boot", d);
    char prev[33] = "";
    bool had = storage_get_str(STORAGE_NS_TEL, KEY_FW, prev, sizeof(prev));
    if (!had || strcmp(prev, fw) != 0) {
        if (had) {
            int ok = rolled_back_from(prev) ? 0 : 1;
            snprintf(d, sizeof(d), "{\"from\":\"%s\",\"to\":\"%s\",\"ok\":%d}", prev, fw, ok);
            telemetry_log("ota", d);
        }
        storage_set_str(STORAGE_NS_TEL, KEY_FW, fw);
    }

    if (xTaskCreate(tel_task, "telemetry", TASK_STACK, NULL, TASK_PRIO, &s_task) != pdPASS) {
        ESP_LOGE(TAG, "sin tarea: telemetria apagada");
        s_task = NULL;
        return;
    }
    ESP_LOGI(TAG, "gen %" PRIu32 ", arranque %" PRIu32 ", acked %" PRIu32, s_gen, s_boot, s_acked);
}

void telemetry_start(void)
{
    if (!s_task) return;
    app_events_subscribe(TWIN_EVT_MQTT, on_mqtt, NULL);
    app_events_subscribe(TWIN_EVT_MQTT_PUBACK, on_puback, NULL);
    if (mqtt_link_connected()) post_ctl(CTL_MQTT_UP, 0); /* por si ya conecto */
}

/* ------------------------------------------------------------------- API */

void telemetry_log(const char *kind, const char *d_json)
{
    if (!s_evq) return;
    if (!kind_ok(kind)) {
        ESP_LOGW(TAG, "tipo de evento no valido: descartado");
        return;
    }
    if (s_demo && strcmp(kind, "demo") != 0) return;
    msg_t m;
    m.t = epoch_or_0();
    m.up = (uint32_t)(esp_timer_get_time() / 1000000LL);
    strncpy(m.kind, kind, sizeof(m.kind)); /* rellena con ceros */
    if (d_json && !d_ok(d_json)) {
        ESP_LOGW(TAG, "%s: d no es un objeto JSON valido de < %d B: va {}", kind, TEL_D_MAX);
        d_json = NULL;
    }
    strncpy(m.d, d_json ? d_json : "{}", sizeof(m.d));
    if (xQueueSend(s_evq, &m, 0) != pdTRUE) {
        drops_add(1);
        ESP_LOGW(TAG, "cola RAM llena: evento %s perdido", kind);
        return;
    }
    if (s_task) xTaskNotifyGive(s_task);
}

void telemetry_set_demo(bool on)
{
    bool was = s_demo;
    s_demo = on;
    if (on && !was) post_ctl(CTL_DEMO_ON, 0);
}

uint32_t telemetry_gen(void) { return s_gen; }
uint32_t telemetry_boot(void) { return s_boot; }
uint32_t telemetry_demo_count(void) { return s_demo_n; }

uint32_t telemetry_pending(void)
{
    uint32_t ram = s_evq ? (uint32_t)uxQueueMessagesWaiting(s_evq) : 0;
    return (s_open ? tel_queue_pending(&s_q) : 0) + ram;
}

uint32_t telemetry_lost(void)
{
    return s_drops + (s_open ? tel_queue_lost(&s_q) : 0);
}

void telemetry_flush_now(uint32_t wait_ms)
{
    if (!s_task) return;
    xSemaphoreTake(s_flush_sem, 0); /* uno viejo sin recoger */
    post_ctl(CTL_FLUSH, 0);
    if (xSemaphoreTake(s_flush_sem, pdMS_TO_TICKS(wait_ms)) != pdTRUE) {
        ESP_LOGW(TAG, "flush: sin PUBACK en %" PRIu32 " ms (los eventos siguen en la cola)", wait_ms);
    }
}

void telemetry_print_status(void)
{
    if (!s_task) {
        printf("telemetria apagada\n");
        return;
    }
    uint32_t ram = (uint32_t)uxQueueMessagesWaiting(s_evq);
    printf("gen %" PRIu32 "  boot_n %" PRIu32 "  next_seq %" PRIu32 "  pending %" PRIu32 "  lost %" PRIu32
           "  acked %" PRIu32 "\n",
           s_gen, s_boot, s_open ? tel_queue_next_seq(&s_q) : 0, telemetry_pending(), telemetry_lost(), s_acked);
    printf("cola %s (%" PRIu32 " ms)  ram %" PRIu32 "/%d  broker %s  demo %s (demo_n %" PRIu32 ")\n",
           s_open ? "abierta" : (s_part ? "abriendo" : "sin particion"), s_open_ms, ram, EVQ_LEN,
           s_connected ? "si" : "no", s_demo ? "si" : "no", s_demo_n);
    int id = s_inflight_id;
    if (id >= 0) {
        printf("en vuelo: msg_id %d, seq %" PRIu32 "..%" PRIu32 ", hace %" PRId64 " s\n", id, s_inflight_first,
               s_inflight_last, (esp_timer_get_time() - s_inflight_us) / 1000000LL);
    } else {
        printf("en vuelo: ninguno\n");
    }
}
