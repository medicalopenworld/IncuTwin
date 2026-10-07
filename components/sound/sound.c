#include "sound.h"

#include <stdatomic.h>

#include "app_events.h"
#include "board.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "settings.h"
#include "twin_types.h"

static const char *TAG = "sound";

typedef struct {
    uint16_t freq;   /* Hz, 0 = silencio */
    uint16_t dur_ms;
} note_t;

/* C4 262 · E4 330 · C5 523 · E5 659 · G5 784 · C6 1047 · E6 1319 · G6 1568 · C7 2093 */
static const note_t BOOT_MELODY[] = { {523, 70}, {659, 70}, {784, 70}, {1047, 180} };
static const note_t BABY_MELODY[] = { {784, 90}, {1047, 90}, {1319, 220} };
static const note_t PARENTS_MELODY[] = {
    {523, 80},  {659, 80},  {784, 80},  {1047, 80},
    {659, 80},  {784, 80},  {1047, 80}, {1319, 80},
    {784, 80},  {1047, 80}, {1319, 80}, {1568, 200},
    {0, 60},    {1568, 80}, {0, 40},    {2093, 400},
};
static const note_t TEST_MELODY[] = { {1047, 120} };
static const note_t CLICK_MELODY[] = { {1760, 25} }; /* A6, feedback tactil */

#define HB_LUB_HZ    330
#define HB_LUB_MS    50
#define HB_GAP_MS    110
#define HB_DUB_HZ    262
#define HB_DUB_MS    40
#define HB_REST_MIN  50
#define HB_WINDOW_MS 3000

/* duty (10 bits) por nivel; a calibrar en hardware */
static const uint16_t VOLUME_DUTY[] = { 0, 8, 20, 512 };

/* peticiones desde otras tareas */
static atomic_int s_req = SOUND_NONE;
static atomic_bool s_hb_window_req = false;
static atomic_uint s_bpm = 0;
static atomic_uint s_volume = 3;
static atomic_bool s_holding = false;

/* estado de la tarea LVGL */
static const note_t *s_seq;
static uint8_t s_seq_len, s_seq_idx;
static uint32_t s_note_end;
static uint32_t s_hb_until;   /* 0 = ventana cerrada */
static uint8_t s_hb_phase;
static uint32_t s_hb_next;
static bool s_hb_idle = true;

/* ------------------------------------------------------------------ tonos */

static void tone(uint16_t freq)
{
    unsigned vol = atomic_load(&s_volume);
    if (freq == 0 || vol == 0) {
        board_buzzer_tone(0, 0);
        return;
    }
    board_buzzer_tone(freq, VOLUME_DUTY[vol > 3 ? 3 : vol]);
}

static void seq_start(const note_t *seq, uint8_t len, uint32_t now)
{
    if (atomic_load(&s_volume) == 0) return;
    s_seq = seq;
    s_seq_len = len;
    s_seq_idx = 0;
    s_note_end = now + seq[0].dur_ms;
    tone(seq[0].freq);
}

static void seq_stop(void)
{
    s_seq = NULL;
    tone(0);
}

static void play(sound_melody_t m, uint32_t now)
{
    switch (m) {
    case SOUND_BOOT:    seq_start(BOOT_MELODY, 4, now); break;
    case SOUND_BABY:    seq_start(BABY_MELODY, 3, now); break;
    case SOUND_PARENTS: seq_start(PARENTS_MELODY, 16, now); break;
    case SOUND_TEST:    seq_start(TEST_MELODY, 1, now); break;
    case SOUND_CLICK:
        if (s_seq) break; /* no cortar una melodia por un tic */
        seq_start(CLICK_MELODY, 1, now);
        break;
    default: break;
    }
}

static bool hb_active(uint32_t now)
{
    if (atomic_load(&s_volume) == 0 || atomic_load(&s_bpm) == 0) return false;
    if (atomic_load(&s_holding)) return true;
    return s_hb_until != 0 && (int32_t)(now - s_hb_until) < 0;
}

static void hb_step(uint32_t now)
{
    if ((int32_t)(now - s_hb_next) < 0) return;
    unsigned bpm = atomic_load(&s_bpm);
    uint32_t period = bpm ? 60000UL / bpm : 1000;
    switch (s_hb_phase) {
    case 0: tone(HB_LUB_HZ); s_hb_next = now + HB_LUB_MS; break;
    case 1: tone(0);         s_hb_next = now + HB_GAP_MS; break;
    case 2: tone(HB_DUB_HZ); s_hb_next = now + HB_DUB_MS; break;
    default: {
        tone(0);
        uint32_t used = HB_LUB_MS + HB_GAP_MS + HB_DUB_MS;
        s_hb_next = now + (period > used + HB_REST_MIN ? period - used : HB_REST_MIN);
        break;
    }
    }
    s_hb_phase = (s_hb_phase + 1) & 3;
}

/* ------------------------------------------------------------------- tick */

static void sound_tick(lv_timer_t *t)
{
    (void)t;
    uint32_t now = lv_tick_get();

    int req = atomic_exchange(&s_req, SOUND_NONE);
    if (req != SOUND_NONE) {
        play((sound_melody_t)req, now);
        s_hb_idle = true;
        s_hb_phase = 0;
    }
    if (atomic_exchange(&s_hb_window_req, false)) {
        s_hb_until = now + HB_WINDOW_MS;
        if (s_hb_until == 0) s_hb_until = 1;
    }
    if (atomic_load(&s_volume) == 0 && (s_seq || !s_hb_idle)) {
        seq_stop(); /* apagado: cortar lo que suene */
        s_hb_until = 0;
        s_hb_idle = true;
    }

    if (s_seq) { /* una melodia tiene prioridad sobre el latido */
        if ((int32_t)(now - s_note_end) < 0) return;
        s_seq_idx++;
        if (s_seq_idx >= s_seq_len) {
            seq_stop();
            s_hb_idle = true; /* reanudar el latido desde un "lub" limpio */
            s_hb_phase = 0;
        } else {
            tone(s_seq[s_seq_idx].freq);
            s_note_end = now + s_seq[s_seq_idx].dur_ms;
        }
        return;
    }

    if (hb_active(now)) {
        if (s_hb_idle) {
            s_hb_idle = false;
            s_hb_phase = 0;
            s_hb_next = now;
        }
        hb_step(now);
    } else if (!s_hb_idle) {
        s_hb_idle = true;
        s_hb_phase = 0;
        tone(0);
    }
}

/* ----------------------------------------------------------------- events */

static void on_transition(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    twin_transition_t tr = *(twin_transition_t *)data;
    if (tr == TWIN_TR_BABY_IN) {
        sound_request(SOUND_BABY);
        atomic_store(&s_hb_window_req, true);
    } else if (tr == TWIN_TR_BABY_PARENTS) {
        sound_request(SOUND_PARENTS);
    }
}

static void on_state(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    const twin_snapshot_t *s = data;
    atomic_store(&s_bpm, s->show_baby ? s->inc.bpm : 0); /* el latido audible sigue al visible */
}

static void on_settings(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    const app_evt_settings_t *st = data;
    atomic_store(&s_volume, st->volume);
}

/* -------------------------------------------------------------------- API */

void sound_init(void)
{
    atomic_store(&s_volume, settings_volume());
    board_buzzer_tone(0, 0);
    if (lvgl_port_lock(1000)) {
        lv_timer_create(sound_tick, 15, NULL);
        lvgl_port_unlock();
    } else {
        ESP_LOGE(TAG, "sin lock de LVGL: sonido deshabilitado");
    }
    app_events_subscribe(TWIN_EVT_TRANSITION, on_transition, NULL);
    app_events_subscribe(TWIN_EVT_STATE_CHANGED, on_state, NULL);
    app_events_subscribe(TWIN_EVT_SETTINGS_CHANGED, on_settings, NULL);
}

void sound_request(sound_melody_t m) { atomic_store(&s_req, (int)m); }

void sound_hand_hold(bool holding) { atomic_store(&s_holding, holding); }
