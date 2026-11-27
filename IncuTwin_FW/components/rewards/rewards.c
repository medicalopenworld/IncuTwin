#include "rewards.h"

#include <string.h>
#include <time.h>

#include "app_events.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "net_wifi.h"
#include "rewards_tel.h"
#include "sdkconfig.h"
#include "stays.h"
#include "storage.h"
#include "telemetry.h"

static const char *TAG = "rewards";

#define KEY_STATE    "state2" /* RW_VERSION 2 */
#define KEY_STATE_V1 "state"  /* version 1: se migra y se deja (por si el OTA hace rollback) */
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
static void post(const rw_news_t *n, bool demo);
static int s_force_month, s_force_mday; /* fecha forzada desde la consola (pruebas); 0 = la de verdad */

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

bool rewards_local_md(int *month, int *mday)
{
    if (s_force_month) {
        *month = s_force_month;
        *mday = s_force_mday;
        return true;
    }
    int32_t day;
    struct tm tm;
    if (!today(&day, &tm)) {
        *month = *mday = 0;
        return false;
    }
    *month = tm.tm_mon + 1;
    *mday = tm.tm_mday;
    return true;
}

void rewards_force_date(int month, int mday)
{
    s_force_month = month >= 1 && month <= 12 ? month : 0;
    s_force_mday = s_force_month ? mday : 0;
    post(NULL, s_demo_on); /* que la pantalla lo repinte (Navidad) */
}

static void save_locked(void)
{
    if (!storage_set_blob(REWARDS_NS, KEY_STATE, &s_real, sizeof(s_real))) ESP_LOGW(TAG, "no se pudo guardar");
}

static void post(const rw_news_t *n, bool demo)
{
    app_evt_rewards_t ev = { .hearts = n ? n->hearts : 0, .new_seals = n ? n->new_seals : 0,
                             .level_up = n && n->level_up, .garland = n && n->garland,
                             .demo = demo };
    app_events_post(TWIN_EVT_REWARDS_CHANGED, &ev, sizeof(ev));
}

static void log_news(const rw_news_t *n, const rw_state_t *s, const char *what)
{
    ESP_LOGI(TAG, "%s: +%u corazones (saldo %u, ganados %lu, racha %u, abrazos %u, sellos 0x%02x%s%s)", what,
             n->hearts, s->balance, (unsigned long)s->earned, s->streak, s->saved, s->seals,
             n->level_up ? ", sube de nivel" : "", n->garland ? ", guirnalda" : "");
}

static void tel_sink(const char *kind, const char *d, void *ctx)
{
    (void)ctx;
    telemetry_log(kind, d);
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
    rw_state_t before = s_real;
    if (in == APP_ACC_GESTURE) {
        rw_gesture_t g = { .time_valid = valid, .day = day, .year = tm.tm_year + 1900, .month = tm.tm_mon + 1,
                           .mday = tm.tm_mday, .rest_mask = s_rest, .country = s_country, .stay = s_stay };
        n = rw_on_gesture(&s_real, &g);
    } else {
        n = rw_on_super(&s_real, valid, day);
    }
    tel_rewards_diff(&before, &s_real, &n, in == APP_ACC_GESTURE ? "gesture" : "super", tel_sink, NULL);
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
        rw_state_t before = s_real;
        n = rw_on_home(&s_real, s_stay);
        tel_rewards_diff(&before, &s_real, &n, "home", tel_sink, NULL);
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
        static rw_state_v1_t v1; /* 140 B: fuera de la pila */
        if (storage_get_blob(REWARDS_NS, KEY_STATE_V1, &v1, sizeof(v1)) && rw_migrate_v1(&s_real, &v1)) {
            ESP_LOGI(TAG, "migrado de la version 1");
            save_locked();
        } else {
            rw_init(&s_real); /* no existe o corrupto */
            s_seed_pending = true;
        }
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

rw_unlock_t rewards_unlock(rw_item_t item, uint16_t *missing)
{
    if (missing) *missing = 0;
    if (!s_mutex) return RW_UNLOCK_NOT_FOR_SALE;
    int month, mday;
    rewards_local_md(&month, &mday);
    LOCK();
    bool demo = s_demo_on;
    if (demo && item == RW_ITEM_NAMED_INCUNEST) { /* en la demo no se canjea de verdad */
        UNLOCK();
        return RW_UNLOCK_NOT_FOR_SALE;
    }
    rw_state_t before = s_real;
    rw_unlock_t res = rw_unlock(demo ? &s_demo : &s_real, item, month, mday, missing);
    bool ok = res == RW_UNLOCK_OK;
    if (ok && !demo) {
        save_locked();
        /* Solo con exito: `spend` sale del diff (baja el saldo y aparece el bit: una compra de verdad,
         * uno por compra; elegir o poner/quitar algo ya suyo no cambia saldo ni bits y no emite). */
        tel_rewards_diff(&before, &s_real, NULL, "unlock", tel_sink, NULL);
        if (item == RW_ITEM_NAMED_INCUNEST) telemetry_log("redeem", "{\"item\":\"named_incunest\"}");
    }
    UNLOCK();
    if (ok) post(NULL, demo);
    return res;
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
    rw_state_t before = s_real;
    rw_news_t n = rw_add(demo ? &s_demo : &s_real, hearts);
    if (!demo) {
        save_locked();
        tel_rewards_diff(&before, &s_real, &n, "incupray", tel_sink, NULL);
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

bool rewards_catchup_take(rw_level_t *level)
{
    if (!s_mutex) return false;
    LOCK();
    bool due = !s_demo_on && rw_catchup_due(&s_real);
    if (due) {
        if (level) *level = rw_level(s_real.earned);
        rw_catchup_done(&s_real);
        save_locked();
        ESP_LOGI(TAG, "aviso de nivel pendiente: nivel %d", (int)rw_level(s_real.earned));
    }
    UNLOCK();
    return due;
}

void rewards_demo_set_level(rw_level_t level)
{
    if (!s_mutex) return;
    LOCK();
    bool demo = s_demo_on;
    rw_news_t n = { 0 };
    if (demo) {
        n.level_up = rw_level(s_demo.earned) < level; /* subir en la demo se anuncia como subir de verdad */
        s_demo.earned = rw_level_min(level);
        s_demo.level_seen = (uint8_t)rw_level(s_demo.earned); /* sin aviso pendiente */
    }
    UNLOCK();
    if (demo) post(&n, true);
}
