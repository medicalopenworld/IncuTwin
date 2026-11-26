#include "stays.h"

#include <string.h>
#include <time.h>

#include "app_events.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include "storage.h"

static const char *TAG = "stays";

#define KEY_LOG     "log"
#if CONFIG_INCUTWIN_SIM
#define STAYS_NS    STORAGE_NS_STAYS_SIM
#else
#define STAYS_NS    STORAGE_NS_STAYS
#endif
#define EPOCH_VALID 1700000000u /* antes de esto el reloj no esta en hora (SNTP pendiente) */

static SemaphoreHandle_t s_mutex;
static stay_log_t s_log;

#define LOCK()   xSemaphoreTake(s_mutex, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_mutex)

static void on_state(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)id;
    const twin_snapshot_t *s = data;
    if (!s || s->demo) return; /* la demo no es una estancia de verdad */
    const twin_incubator_t *inc = &s->inc;
    time_t now = time(NULL);
    /* cuidado implica bebe: con calor o fototerapia hay bebe aunque no este registrado */
    bool care = inc->thermo != TWIN_THERMO_OFF || inc->photo;
    stay_obs_t o = {
        .inc = s->link.paired ? s->link.incubator_id : "",
        .country = inc->country,
        .stay_id = inc->stay_id,
        .present = s->link_ok && (inc->baby == TWIN_BABY_IN || inc->baby == TWIN_BABY_PARENTS || care),
        .known = s->link_ok,
        .sex = (uint8_t)inc->sex,
        .now = now >= (time_t)EPOCH_VALID ? (uint32_t)now : 0,
    };
    LOCK();
    bool changed = stay_log_observe(&s_log, &o);
    if (changed && !storage_set_blob(STAYS_NS, KEY_LOG, &s_log, sizeof(s_log))) {
        ESP_LOGW(TAG, "no se pudo guardar");
    }
    UNLOCK();
    if (changed) {
        ESP_LOGI(TAG, "bebes de las IncuNest actualizados (%u estancias)", s_log.count);
        app_events_post(TWIN_EVT_STAYS_CHANGED, NULL, 0);
    }
}

void stays_init(void)
{
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    LOCK();
    if (!storage_get_blob(STAYS_NS, KEY_LOG, &s_log, sizeof(s_log)) || s_log.head >= STAY_LOG_MAX ||
        s_log.count > STAY_LOG_MAX) {
        memset(&s_log, 0, sizeof(s_log)); /* no existe, de otro tamano o corrupto */
    }
    UNLOCK();
    app_events_subscribe(TWIN_EVT_STATE_CHANGED, on_state, NULL);
}

uint32_t stays_open_key(const char *inc)
{
    if (!s_mutex) return 0;
    LOCK();
    uint32_t k = stay_log_open_key(&s_log, inc);
    UNLOCK();
    return k;
}

void stays_copy(stay_log_t *out)
{
    if (!s_mutex) {
        memset(out, 0, sizeof(*out));
        return;
    }
    LOCK();
    *out = s_log;
    UNLOCK();
}
