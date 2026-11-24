#include "event_log.h"

#include <string.h>
#include <time.h>

#include "app_events.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "storage.h"

static const char *TAG = "evlog";

#define KEY_RING     "ring"
#define EPOCH_VALID  1700000000u /* antes de esto el reloj no esta en hora (SNTP pendiente) */

static SemaphoreHandle_t s_mutex;
static event_ring_t s_ring;

#define LOCK()   xSemaphoreTake(s_mutex, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_mutex)

static void save_locked(void)
{
    if (!storage_set_blob(STORAGE_NS_EVLOG, KEY_RING, &s_ring, sizeof(s_ring))) {
        ESP_LOGW(TAG, "no se pudo guardar el historial");
    }
}

static void on_notice(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)id;
    const app_evt_notice_t *n = data;
    if (!n || n->demo) return;
    event_log_add(n->mask, n->inc);
}

void event_log_init(void)
{
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    LOCK();
    memset(&s_ring, 0, sizeof(s_ring));
    if (!storage_get_blob(STORAGE_NS_EVLOG, KEY_RING, &s_ring, sizeof(s_ring))) {
        memset(&s_ring, 0, sizeof(s_ring)); /* no existe o de otro tamano */
    } else if (s_ring.head >= EVENT_LOG_MAX || s_ring.count > EVENT_LOG_MAX || s_ring.unseen > EVENT_LOG_MAX) {
        memset(&s_ring, 0, sizeof(s_ring)); /* blob corrupto */
    }
    UNLOCK();
    app_events_subscribe(TWIN_EVT_NOTICE, on_notice, NULL);
}

void event_log_add(uint32_t notices_mask, const char *inc)
{
    if (!s_mutex || !notices_mask) return;
    time_t now = time(NULL);
    uint32_t at = now >= (time_t)EPOCH_VALID ? (uint32_t)now : 0;
    bool changed = false;
    LOCK();
    for (uint8_t b = 0; b < 8; b++) {
        if (!(notices_mask & (1u << b))) continue;
        event_entry_t e = { .at = at, .kind = (uint8_t)(1u << b) };
        if (inc) {
            strncpy(e.inc, inc, sizeof(e.inc) - 1);
        }
        event_ring_push(&s_ring, &e);
        changed = true;
    }
    if (changed) save_locked();
    UNLOCK();
    if (changed) app_events_post(TWIN_EVT_LOG_CHANGED, NULL, 0);
}

uint8_t event_log_count(void)
{
    if (!s_mutex) return 0;
    LOCK();
    uint8_t n = s_ring.count;
    UNLOCK();
    return n;
}

bool event_log_get(uint8_t i, event_entry_t *out)
{
    if (!s_mutex) return false;
    LOCK();
    bool ok = event_ring_get(&s_ring, i, out);
    UNLOCK();
    return ok;
}

uint8_t event_log_unseen(void)
{
    if (!s_mutex) return 0;
    LOCK();
    uint8_t n = s_ring.unseen;
    UNLOCK();
    return n;
}

void event_log_mark_seen(void)
{
    if (!s_mutex) return;
    LOCK();
    bool changed = s_ring.unseen != 0;
    event_ring_mark_seen(&s_ring);
    if (changed) save_locked();
    UNLOCK();
    if (changed) app_events_post(TWIN_EVT_LOG_CHANGED, NULL, 0);
}
