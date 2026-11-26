#include "incupray.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_events.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mqtt_link.h"
#include "net_wifi.h"
#include "rewards.h"
#include "storage.h"
#include "telemetry.h"

static const char *TAG = "incupray";

#define KEY_STATE   "state"
#define REPORT_S    60
#define DEMO_LEN_S  180
#define EPOCH_VALID 1700000000u

/* Lo que se recuerda de cada IncuPray (NVS), para no cobrar ni avisar dos veces tras reiniciar */
typedef struct {
    char id[IP_ID_MAX + 1];
    uint8_t shown;    /* avisos enseñados (ip_notice_t) */
    uint8_t paid;     /* apuesta cobrada (0 o 3) */
    uint8_t joined;   /* entro alguna vez */
    uint8_t hearts;   /* corazones ya pagados */
    uint32_t held_s;  /* segundos con el dedo */
    char summary_seen[IP_ID_MAX + 1]; /* resumen ya enseñado (o la IncuPray en la que se entro) */
} ip_saved_t;

static SemaphoreHandle_t s_mutex;
static ip_window_t s_win;
static bool s_has_win;
static ip_live_t s_live;            /* 2 kB: estatico */
static ip_summary_t s_sum;
static bool s_has_sum;
static ip_saved_t s_saved;
static bool s_in;                   /* dentro de la IncuPray ahora */
static uint32_t s_hold_ms;          /* milisegundos sueltos (menos de 1 s) */
static esp_timer_handle_t s_report_timer;
/* demo */
static bool s_demo;
static ip_window_t s_demo_win;
static ip_live_t s_demo_live;
static ip_saved_t s_demo_saved;

#define LOCK()   xSemaphoreTake(s_mutex, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_mutex)

static ip_saved_t *saved(void) { return s_demo ? &s_demo_saved : &s_saved; }
static const ip_window_t *win(void) { return s_demo ? &s_demo_win : (s_has_win ? &s_win : NULL); }

static void save_locked(void)
{
    if (s_demo) return;
    if (!storage_set_blob(STORAGE_NS_INCUPRAY, KEY_STATE, &s_saved, sizeof(s_saved))) ESP_LOGW(TAG, "no se pudo guardar");
}

static void notify(void) { app_events_post(TWIN_EVT_INCUPRAY, NULL, 0); }

static uint32_t uptime_s(void) { return (uint32_t)(esp_timer_get_time() / 1000000) + 1; }

static uint32_t epoch_now(void)
{
    if (!net_wifi_time_valid()) return 0;
    time_t t = time(NULL);
    return t >= (time_t)EPOCH_VALID ? (uint32_t)t : 0;
}

/* En la demo la hora es la del arranque (en una feria puede no haber SNTP) */
uint32_t incupray_now(void) { return s_demo ? uptime_s() : epoch_now(); }

/* Una IncuPray nueva (otro id): empieza su memoria */
static void track_locked(const char *id)
{
    if (!strncmp(s_saved.id, id, sizeof(s_saved.id))) return;
    char seen[IP_ID_MAX + 1];
    memcpy(seen, s_saved.summary_seen, sizeof(seen));
    memset(&s_saved, 0, sizeof(s_saved));
    snprintf(s_saved.id, sizeof(s_saved.id), "%s", id);
    memcpy(s_saved.summary_seen, seen, sizeof(seen));
    save_locked();
}

/* ------------------------------------------------------------- broker */

/* Telemetria `incupray`: ev = join|leave|close (NULL = el aviso periodico, sin evento). held_s y
 * hearts son los acumulados de esa IncuPray; los corazones ya salen como `heart` (why incupray)
 * desde rewards, aqui solo se resume. join lleva ademas `paid`, la apuesta cobrada al entrar (0 o
 * 3): rewards_spend no emite evento, asi que es la unica pista para analizar la apuesta. Nunca en la
 * demo (s_demo ya corta antes). */
static void tel_incupray(const char *ev)
{
    for (const char *c = s_saved.id; *c; c++) {
        bool ok = (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') ||
                  *c == '.' || *c == '_' || *c == '-';
        if (!ok) return; /* id raro: sin escapar no se registra */
    }
    char d[136];
    char paid[16] = "";
    if (strcmp(ev, "join") == 0) snprintf(paid, sizeof paid, ",\"paid\":%u", (unsigned)s_saved.paid);
    snprintf(d, sizeof d, "{\"id\":\"%s\",\"ev\":\"%s\",\"held_s\":%lu,\"hearts\":%u%s}", s_saved.id, ev,
             (unsigned long)s_saved.held_s, (unsigned)s_saved.hearts, paid);
    telemetry_log("incupray", d);
}

static void publish_participation_locked(const char *why, const char *tel_ev)
{
    if (s_demo || !s_saved.joined) return;
    if (tel_ev) tel_incupray(tel_ev);
    char topic[80], p[IP_PARTICIPATION_MAX + 1];
    int n = ip_participation_build(p, sizeof(p), s_saved.id, s_saved.held_s, s_saved.hearts);
    if (n < 0) return;
    snprintf(topic, sizeof(topic), "incutwin/%s/incupray", mqtt_link_client_id());
    if (mqtt_link_publish(topic, p, (size_t)n, 1, false)) ESP_LOGI(TAG, "participacion (%s): %s", why, p);
}

static void report_cb(void *arg)
{
    (void)arg;
    LOCK();
    if (s_in && !s_demo) publish_participation_locked("cada minuto", NULL);
    /* la ventana se ha cerrado: fuera */
    const ip_window_t *w = win();
    if (s_in && w && !s_demo && ip_phase(w, epoch_now()) == IP_AFTER) {
        s_in = false;
        publish_participation_locked("cierre", "close");
    }
    UNLOCK();
}

bool incupray_is_topic(const char *topic)
{
    return topic && !strncmp(topic, "incupray/", 9);
}

void incupray_ingest(const char *topic, const char *payload, size_t len)
{
    if (!s_mutex || !incupray_is_topic(topic)) return;
    const char *what = topic + 9;
    LOCK();
    bool changed = false;
    if (!strcmp(what, "next")) {
        ip_window_t w;
        if (len == 0) { /* horario borrado: IncuPray apagada */
            changed = s_has_win;
            s_has_win = false;
            ESP_LOGI(TAG, "sin horario: IncuPray apagada");
        } else if (ip_parse_next(payload, len, &w)) {
            changed = !s_has_win || memcmp(&w, &s_win, sizeof(w)) != 0;
            s_win = w;
            s_has_win = true;
            track_locked(w.id);
            if (changed) ESP_LOGI(TAG, "proxima IncuPray %s: inicio %lu, %lu s", w.id, (unsigned long)w.start,
                                  (unsigned long)w.duration_s);
        } else {
            ESP_LOGW(TAG, "incupray/next no valido");
        }
    } else if (!strcmp(what, "live")) {
        static ip_live_t l;
        if (ip_parse_live(payload, len, &l)) {
            s_live = l;
            changed = true;
        }
    } else if (!strcmp(what, "summary")) {
        ip_summary_t s;
        if (ip_parse_summary(payload, len, &s)) {
            s_sum = s;
            s_has_sum = true;
            changed = true;
            ESP_LOGI(TAG, "resumen %s: %u IncuTwin, %lu corazones", s.id, s.count, (unsigned long)s.hearts);
        }
    }
    UNLOCK();
    if (changed) notify();
}

/* ------------------------------------------------------------------ API */

bool incupray_window(ip_window_t *out)
{
    if (!s_mutex) return false;
    LOCK();
    const ip_window_t *w = win();
    if (w) *out = *w;
    UNLOCK();
    return w != NULL;
}

void incupray_live(ip_live_t *out)
{
    if (!s_mutex) {
        memset(out, 0, sizeof(*out));
        return;
    }
    LOCK();
    *out = s_demo ? s_demo_live : s_live;
    UNLOCK();
}

bool incupray_demo_active(void) { return s_demo; }

ip_notice_t incupray_take_notice(void)
{
    if (!s_mutex) return IP_NOTICE_NONE;
    LOCK();
    const ip_window_t *w = win();
    ip_notice_t n = w ? ip_notice_due(w, incupray_now(), saved()->shown) : IP_NOTICE_NONE;
    if (n != IP_NOTICE_NONE) {
        saved()->shown |= (uint8_t)n;
        save_locked();
    }
    UNLOCK();
    return n;
}

bool incupray_take_missed_summary(ip_summary_t *out)
{
    if (!s_mutex) return false;
    LOCK();
    bool r = false;
    if (s_has_sum && !s_demo && strncmp(s_sum.id, s_saved.summary_seen, sizeof(s_sum.id)) != 0 &&
        !(s_saved.joined && !strncmp(s_saved.id, s_sum.id, sizeof(s_sum.id)))) {
        *out = s_sum;
        snprintf(s_saved.summary_seen, sizeof(s_saved.summary_seen), "%s", s_sum.id);
        save_locked();
        r = true;
    }
    UNLOCK();
    return r;
}

bool incupray_summary(ip_summary_t *out)
{
    if (!s_mutex) return false;
    LOCK();
    bool r = s_has_sum && !s_demo;
    if (r) *out = s_sum;
    UNLOCK();
    return r;
}

bool incupray_join(uint16_t *paid)
{
    if (paid) *paid = 0;
    if (!s_mutex) return false;
    LOCK();
    ip_saved_t *sv = saved();
    s_in = true;
    bool first = !sv->joined;
    if (first) { /* la primera vez en esta IncuPray: la apuesta, si hay saldo */
        sv->joined = 1;
        rw_state_t r;
        rewards_copy(&r, NULL);
        uint16_t stake = ip_stake(r.balance);
        /* la apuesta (rewards_spend) no emite evento de telemetria, a proposito */
        if (stake && rewards_spend(stake)) sv->paid = (uint8_t)stake;
        if (paid) *paid = sv->paid;
        if (!s_demo) snprintf(s_saved.summary_seen, sizeof(s_saved.summary_seen), "%s", s_saved.id);
        save_locked();
        ESP_LOGI(TAG, "dentro de la IncuPray %s (apuesta %u)", sv->id, sv->paid);
    }
    publish_participation_locked("al entrar", "join");
    UNLOCK();
    notify();
    return first;
}

void incupray_leave(void)
{
    if (!s_mutex) return;
    LOCK();
    if (s_in) publish_participation_locked("al salir", "leave");
    s_in = false;
    if (s_demo) { /* la demo acaba al salir */
        s_demo = false;
        ESP_LOGI(TAG, "IncuPray de la demo terminada");
    }
    UNLOCK();
    notify();
}

bool incupray_joined(void) { return s_in; }

void incupray_hold(uint32_t ms)
{
    if (!s_mutex || !ms) return;
    LOCK();
    const ip_window_t *w = win();
    uint8_t pay = 0;
    if (s_in && w && ip_phase(w, incupray_now()) == IP_OPEN) {
        ip_saved_t *sv = saved();
        s_hold_ms += ms;
        sv->held_s += s_hold_ms / 1000;
        s_hold_ms %= 1000;
        uint8_t due = ip_hearts_for(sv->held_s);
        if (due > sv->hearts) {
            pay = (uint8_t)(due - sv->hearts);
            sv->hearts = due;
            save_locked();
        }
    }
    UNLOCK();
    if (pay) {
        rewards_add(pay); /* con la demo, al saldo simulado */
        notify();
    }
}

uint32_t incupray_held_s(void) { return s_mutex ? saved()->held_s : 0; }
uint8_t incupray_hearts(void) { return s_mutex ? saved()->hearts : 0; }

/* ------------------------------------------------------------------ demo */

static const char *DEMO_NAMES[] = { "Familia García", "Lucía y Pablo", "Los de Zaragoza", "ITW-3F21A0",
                                    "Ana", "Casa Martín", "Abuela Carmen", "ITW-9C04B2", "Los Pérez",
                                    "Marta y Luis", "Equipo MOW", "ITW-51D7E8" };
static const ip_baby_t DEMO_BABIES[] = {
    { "D1", "CM", TWIN_SEX_FEMALE, 3 }, { "D2", "CM", TWIN_SEX_MALE, 4 }, { "D3", "TG", TWIN_SEX_MALE, 3 },
    { "D4", "SN", TWIN_SEX_FEMALE, 4 }, { "D5", "BO", TWIN_SEX_FEMALE, 2 }, { "D6", "KE", TWIN_SEX_MALE, 4 },
};

void incupray_demo_start(void)
{
    if (!s_mutex) return;
    LOCK();
    s_demo = true;
    memset(&s_demo_saved, 0, sizeof(s_demo_saved));
    snprintf(s_demo_saved.id, sizeof(s_demo_saved.id), "demo");
    s_demo_win = (ip_window_t){ .start = uptime_s(), .duration_s = DEMO_LEN_S };
    snprintf(s_demo_win.id, sizeof(s_demo_win.id), "demo");
    memset(&s_demo_live, 0, sizeof(s_demo_live));
    snprintf(s_demo_live.id, sizeof(s_demo_live.id), "demo");
    s_demo_live.count = 214;
    s_demo_live.hearts = 1380;
    s_demo_live.n_names = (uint8_t)(sizeof(DEMO_NAMES) / sizeof(DEMO_NAMES[0]));
    for (int i = 0; i < s_demo_live.n_names; i++) {
        snprintf(s_demo_live.names[i], IP_NAME_BYTES, "%s", DEMO_NAMES[i]);
    }
    s_demo_live.n_babies = (uint8_t)(sizeof(DEMO_BABIES) / sizeof(DEMO_BABIES[0]));
    memcpy(s_demo_live.babies, DEMO_BABIES, sizeof(DEMO_BABIES));
    UNLOCK();
    ESP_LOGI(TAG, "IncuPray de la demo: 3 min");
    notify();
}

void incupray_demo_stop(void)
{
    if (!s_mutex || !s_demo) return;
    LOCK();
    s_demo = false;
    s_in = false;
    UNLOCK();
    ESP_LOGI(TAG, "IncuPray de la demo cortada");
    notify();
}

/* ------------------------------------------------------------------ init */

void incupray_init(void)
{
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    LOCK();
    if (!storage_get_blob(STORAGE_NS_INCUPRAY, KEY_STATE, &s_saved, sizeof(s_saved))) memset(&s_saved, 0, sizeof(s_saved));
    UNLOCK();
    const esp_timer_create_args_t t = { .callback = report_cb, .name = "incupray" };
    ESP_ERROR_CHECK(esp_timer_create(&t, &s_report_timer));
    esp_timer_start_periodic(s_report_timer, (uint64_t)REPORT_S * 1000000ULL);
}
