#include "rewards.h"

#include <string.h>
#include <time.h>

#include "app_events.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "net_wifi.h"
#include "sdkconfig.h"
#include "stays.h"
#include "storage.h"

static const char *TAG = "rewards";

#define KEY_STATE "state"
#if CONFIG_INCUTWIN_SIM
#define REWARDS_NS STORAGE_NS_REWARDS_SIM
#else
#define REWARDS_NS STORAGE_NS_REWARDS
#endif

static SemaphoreHandle_t s_mutex;
static rw_state_t s_real, s_demo;
static bool s_demo_on;
static bool s_seed_pending; /* blob nuevo: sembrar la racha con la de accompany en cuanto se pueda */

/* lo ultimo visto del estado (lo usa el gesto) */
static char s_country[TWIN_COUNTRY_LEN];
static uint8_t s_rest;
static uint32_t s_stay;     /* estancia del bebe que se ve ahora (se recuerda al irse a casa) */
static bool s_was_home;

#define LOCK()   xSemaphoreTake(s_mutex, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_mutex)

/* Dia local de España; false sin hora */
static bool today(int32_t *day, struct tm *out)
{
    if (!net_wifi_time_valid()) return false;
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    *day = rw_day_of(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    if (out) *out = tm;
    return true;
}

static void save_locked(void)
{
    if (!storage_set_blob(REWARDS_NS, KEY_STATE, &s_real, sizeof(s_real))) ESP_LOGW(TAG, "no se pudo guardar");
}

static void post(const rw_news_t *n, bool demo)
{
    app_evt_rewards_t ev = { .hearts = n ? n->hearts : 0, .new_seals = n ? n->new_seals : 0,
                             .level_up = n && n->level_up, .garland = n && n->garland, .demo = demo };
    app_events_post(TWIN_EVT_REWARDS_CHANGED, &ev, sizeof(ev));
}

static void log_news(const rw_news_t *n, const rw_state_t *s, const char *what)
{
    ESP_LOGI(TAG, "%s: +%u corazones (saldo %u, ganados %lu, racha %u, abrazos %u, sellos 0x%02x%s%s)", what,
             n->hearts, s->balance, (unsigned long)s->earned, s->streak, s->saved, s->seals,
             n->level_up ? ", sube de nivel" : "", n->garland ? ", guirnalda" : "");
}

static bool news_any(const rw_news_t *n) { return n->hearts || n->new_seals || n->level_up || n->garland; }

static void on_acc_input(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)id;
    app_evt_acc_input_t in = *(const app_evt_acc_input_t *)data;
    if (in != APP_ACC_GESTURE && in != APP_ACC_SUPER) return;
    int32_t day = 0;
    struct tm tm = { 0 };
    bool valid = today(&day, &tm);
    LOCK();
    rw_news_t n;
    if (in == APP_ACC_GESTURE) {
        rw_gesture_t g = { .time_valid = valid, .day = day, .year = tm.tm_year + 1900, .month = tm.tm_mon + 1,
                           .mday = tm.tm_mday, .rest_mask = s_rest, .country = s_country, .stay = s_stay };
        n = rw_on_gesture(&s_real, &g);
    } else {
        n = rw_on_super(&s_real, valid, day);
    }
    bool changed = news_any(&n) || in == APP_ACC_GESTURE; /* el pais o el bebe pueden ser nuevos */
    if (changed) save_locked();
    if (news_any(&n)) log_news(&n, &s_real, in == APP_ACC_GESTURE ? "gesto" : "super corazon");
    UNLOCK();
    if (changed) post(&n, false);
}

static void on_state(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)id;
    const twin_snapshot_t *s = data;
    if (!s) return;
    bool demo_changed = s->demo != s_demo_on;
    if (demo_changed) {
        LOCK();
        s_demo_on = s->demo;
        if (s_demo_on) rw_demo_seed(&s_demo); /* cada demo empieza igual */
        UNLOCK();
        post(NULL, s->demo);
    }
    if (s->demo) return; /* el estado real no se ve durante la demo */

    const char *inc = s->link.paired ? s->link.incubator_id : "";
    bool present = s->link_ok && (s->inc.baby == TWIN_BABY_IN || s->inc.baby == TWIN_BABY_PARENTS);
    bool home = s->link_ok && s->inc.baby == TWIN_BABY_HOME;
    rw_news_t n = { 0 };
    bool save = false;
    LOCK();
    memcpy(s_country, s->inc.country, sizeof(s_country));
    s_rest = s->acc_rest;
    if (present) {
        uint32_t k = stays_open_key(inc); /* stays ya ha visto este estado (se suscribio antes) */
        if (k) s_stay = k;
    }
    if (home && !s_was_home) { /* se fue a casa: el bebe de antes de irse */
        n = rw_on_home(&s_real, s_stay);
        save = news_any(&n);
    }
    s_was_home = home;
    int32_t day;
    if (s_seed_pending && s->acc != TWIN_ACC_OFF && today(&day, NULL)) {
        s_seed_pending = false;
        rw_seed_streak(&s_real, s->acc_week, s->acc_rest, day);
        ESP_LOGI(TAG, "racha sembrada desde acompañar: %u dias", s_real.streak);
        save = true;
    }
    if (save) save_locked();
    if (news_any(&n)) log_news(&n, &s_real, "a casa");
    UNLOCK();
    if (save) post(&n, false);
}

void rewards_init(void)
{
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    LOCK();
    if (!storage_get_blob(REWARDS_NS, KEY_STATE, &s_real, sizeof(s_real)) || s_real.version != RW_VERSION ||
        s_real.n_countries > RW_PASSPORT_MAX) {
        rw_init(&s_real); /* no existe, de otra version o corrupto */
        s_seed_pending = true;
    }
    rw_demo_seed(&s_demo);
    ESP_LOGI(TAG, "saldo %u, ganados %lu, racha %u, sellos 0x%02x, %u paises", s_real.balance,
             (unsigned long)s_real.earned, s_real.streak, s_real.seals, s_real.n_countries);
    UNLOCK();
    app_events_subscribe(TWIN_EVT_ACC_INPUT, on_acc_input, NULL);
    app_events_subscribe(TWIN_EVT_STATE_CHANGED, on_state, NULL);
}

void rewards_copy(rw_state_t *out, bool *demo)
{
    if (!s_mutex) {
        rw_init(out);
        if (demo) *demo = false;
        return;
    }
    LOCK();
    *out = s_demo_on ? s_demo : s_real;
    if (demo) *demo = s_demo_on;
    UNLOCK();
}

bool rewards_unlock(rw_item_t item, uint16_t *missing)
{
    if (!s_mutex) return false;
    LOCK();
    bool demo = s_demo_on;
    if (demo && item == RW_ITEM_NAMED_INCUNEST) { /* en la demo no se canjea de verdad */
        UNLOCK();
        return false;
    }
    bool ok = rw_unlock(demo ? &s_demo : &s_real, item, missing);
    if (ok && !demo) save_locked();
    UNLOCK();
    if (ok) post(NULL, demo);
    return ok;
}

bool rewards_spend(uint16_t n)
{
    if (!s_mutex || !n) return n == 0;
    LOCK();
    bool demo = s_demo_on;
    bool ok = rw_spend(demo ? &s_demo : &s_real, n);
    if (ok && !demo) save_locked();
    UNLOCK();
    if (ok) post(NULL, demo);
    return ok;
}

void rewards_add(uint8_t hearts)
{
    if (!s_mutex || !hearts) return;
    LOCK();
    bool demo = s_demo_on;
    rw_news_t n = rw_add(demo ? &s_demo : &s_real, hearts);
    if (!demo) {
        save_locked();
        log_news(&n, &s_real, "incupray");
    }
    UNLOCK();
    post(&n, demo);
}

void rewards_mark_redeem_sent(void)
{
    if (!s_mutex) return;
    LOCK();
    if (!s_real.redeem_sent) {
        s_real.redeem_sent = 1;
        save_locked();
    }
    UNLOCK();
}

void rewards_demo_gesture(const char *country)
{
    if (!s_mutex) return;
    int32_t day = 0;
    struct tm tm = { 0 };
    bool valid = today(&day, &tm);
    LOCK();
    if (!s_demo_on) {
        UNLOCK();
        return;
    }
    /* la demo cuenta aunque el reloj no este en hora (ferias sin WiFi): un dia ficticio fijo */
    rw_gesture_t g = { .time_valid = true, .day = valid ? day : 1, .year = tm.tm_year + 1900,
                       .month = tm.tm_mon + 1, .mday = tm.tm_mday, .country = country, .stay = 1 };
    rw_news_t n = rw_on_gesture(&s_demo, &g);
    UNLOCK();
    post(&n, true);
}
