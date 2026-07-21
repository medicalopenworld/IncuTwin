#include "sound.h"

#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>

#include "config.h"      /* DATA_STALE_S */
#include "pins_config.h"

#define BUZZER_LEDC_CH 2 /* backlight owns ch 0; 0-1 share a timer */
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    uint16_t freq;   /* Hz, 0 = rest */
    uint16_t dur_ms;
} melody_note_t;

/* retro square-wave jingles (equal temperament) */
static const melody_note_t BOOT_MELODY[] = {
    {523, 70}, {659, 70}, {784, 70}, {1047, 180}}; /* C5 E5 G5 C6 */
static const melody_note_t BABY_MELODY[] = {
    {784, 90}, {1047, 90}, {1319, 220}};           /* G5 C6 E6    */

/* heartbeat "lub-dub" (E4/C4; one octave up if too quiet on hardware) */
#define HB_LUB_HZ 330
#define HB_LUB_MS 50
#define HB_GAP_MS 110
#define HB_DUB_HZ 262
#define HB_DUB_MS 40
#define HB_WINDOW_MS 10000 /* audible heartbeat after baby detected */

static bool s_enabled = true;

/* one-shot melody being played (nullptr = none) */
static const melody_note_t *s_seq = nullptr;
static uint8_t s_seq_len = 0, s_seq_idx = 0;
static uint32_t s_note_end = 0;

/* heartbeat state */
static uint16_t s_bpm = 0;
static bool s_holding = false;
static uint32_t s_hb_until = 0;   /* window deadline (0 = closed)  */
static uint8_t s_hb_phase = 0;    /* 0 lub, 1 gap, 2 dub, 3 rest   */
static uint32_t s_hb_next = 0;
static bool s_hb_idle = true;
static bool s_prev_inside = false;

static void tone_out(uint16_t freq) { ledcWriteTone(BUZZER_LEDC_CH, freq); }

static bool hb_active(void) {
    if (!s_enabled || s_bpm == 0) return false;
    if (s_holding) return true;
    return s_hb_until != 0 && (int32_t)(millis() - s_hb_until) < 0;
}

static void seq_start(const melody_note_t *seq, uint8_t len) {
    if (!s_enabled) return;
    s_seq = seq;
    s_seq_len = len;
    s_seq_idx = 0;
    s_note_end = millis() + seq[0].dur_ms;
    tone_out(seq[0].freq);
}

static void seq_stop(void) {
    s_seq = nullptr;
    tone_out(0);
}

static void hb_step(uint32_t now) {
    if ((int32_t)(now - s_hb_next) < 0) return;
    uint32_t period = 60000UL / s_bpm;
    switch (s_hb_phase) {
        case 0:
            tone_out(HB_LUB_HZ);
            s_hb_next = now + HB_LUB_MS;
            break;
        case 1:
            tone_out(0);
            s_hb_next = now + HB_GAP_MS;
            break;
        case 2:
            tone_out(HB_DUB_HZ);
            s_hb_next = now + HB_DUB_MS;
            break;
        default: { /* rest until the beat period is complete */
            tone_out(0);
            uint32_t used = HB_LUB_MS + HB_GAP_MS + HB_DUB_MS;
            s_hb_next = now + (period > used + 50 ? period - used : 50);
            break;
        }
    }
    s_hb_phase = (s_hb_phase + 1) & 3;
}

static void sound_tick(lv_timer_t *t) {
    (void)t;
    uint32_t now = millis();

    if (s_seq) { /* a one-shot melody has priority */
        if ((int32_t)(now - s_note_end) < 0) return;
        s_seq_idx++;
        if (s_seq_idx >= s_seq_len) {
            seq_stop();
            s_hb_idle = true; /* resume heartbeat from a clean lub */
            s_hb_phase = 0;
        } else {
            tone_out(s_seq[s_seq_idx].freq);
            s_note_end = now + s_seq[s_seq_idx].dur_ms;
        }
        return;
    }

    if (hb_active()) {
        if (s_hb_idle) {
            s_hb_idle = false;
            s_hb_phase = 0;
            s_hb_next = now;
        }
        hb_step(now);
    } else if (!s_hb_idle) { /* just deactivated: silence output */
        s_hb_idle = true;
        s_hb_phase = 0;
        tone_out(0);
    }
}

void sound_init(void) {
    ledcSetup(BUZZER_LEDC_CH, 2000, 10);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CH);
    tone_out(0);

    Preferences p;
    p.begin("incutwin", true);
    s_enabled = p.getBool("sound", true);
    p.end();

    lv_timer_create(sound_tick, 15, nullptr);
}

void sound_set_enabled(bool on) {
    if (on == s_enabled) return;
    s_enabled = on;
    Preferences p;
    p.begin("incutwin", false);
    p.putBool("sound", on);
    p.end();
    if (!on) { /* cut anything currently playing */
        seq_stop();
        s_hb_until = 0;
        s_hb_phase = 0;
        s_hb_idle = true;
    }
}

bool sound_is_enabled(void) { return s_enabled; }

void sound_play_boot(void) {
    seq_start(BOOT_MELODY, ARRAY_LEN(BOOT_MELODY));
}

void sound_on_state(const twin_state_t *st) {
    bool stale = st->last_update_ms == 0 ||
                 (millis() - st->last_update_ms) > (DATA_STALE_S * 1000UL);
    /* same definition ui_apply_state() uses for show_baby */
    bool online = st->wifi_connected && st->cloud_connected &&
                  st->node_seen && st->incubator_online && !stale;

    bool inside = online && st->baby_present;
    s_bpm = inside ? st->heart_rate : 0; /* mirror the on-screen heart */

    if (inside && !s_prev_inside) {
        seq_start(BABY_MELODY, ARRAY_LEN(BABY_MELODY));
        s_hb_until = millis() + HB_WINDOW_MS;
        if (s_hb_until == 0) s_hb_until = 1; /* 0 means "closed" */
    }
    s_prev_inside = inside;
}

void sound_hand_hold(bool holding) { s_holding = holding; }
