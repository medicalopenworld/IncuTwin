#include "demo_mode.h"

#include <Arduino.h>

#include "app/app_state.h"
#include "app/scenarios.h"
#include "pins_config.h"
#include "sound/sound.h"

#define DEMO_DEBOUNCE_MS 30
#define DEMO_HOLD_MS 2000 /* pulsación larga: entra/sale del modo demo */

static bool s_enabled = false;
static bool s_active = false;
static size_t s_idx = 0;
static uint32_t s_seq = 0;
static uint32_t s_last_force = 0;
static twin_state_t s_saved; /* g_state al entrar en demo */

/* botón: antirrebote + distinción corta/larga */
static bool s_pressed = false;   /* nivel ya estable          */
static bool s_raw_last = false;  /* última lectura cruda      */
static uint32_t s_raw_since = 0; /* cuándo cambió esa lectura */
static uint32_t s_press_start = 0;
static bool s_long_done = false; /* la larga ya se consumió   */

/* ------------------------------------------------------------------ estado */

/* La demo va sin red: el panel se pinta a sí mismo como conectado y con
 * datos frescos, que es lo que la UI exige para mostrar al bebé. */
static void force_conn(void) {
    state_lock();
    if (!g_state.wifi_connected || !g_state.cloud_connected) {
        g_state.wifi_connected = true;
        g_state.cloud_connected = true;
        g_state_dirty = true;
    }
    g_state.last_update_ms = millis();
    state_unlock();
}

static void demo_enter(void) {
    state_lock();
    s_saved = g_state;
    state_unlock();

    s_active = true;
    s_idx = 0;
    s_seq++;
    s_last_force = millis();
    scenario_apply_idx(s_idx);
    force_conn();
    Serial.printf("[demo] ON escenario '%s' t=%lu\n", scenario_id(s_idx),
                  (unsigned long)millis());
}

static void demo_exit(void) {
    s_active = false;
    s_seq++;
    state_lock();
    g_state = s_saved; /* vuelve el estado real (probablemente "sin WiFi") */
    g_state_dirty = true;
    state_unlock();
    Serial.printf("[demo] OFF t=%lu\n", (unsigned long)millis());
}

static void demo_next(void) {
    s_idx = (s_idx + 1) % scenario_count();
    s_seq++;
    scenario_apply_idx(s_idx);
    force_conn();
    Serial.printf("[demo] escenario '%s' (%u/%u)\n", scenario_id(s_idx),
                  (unsigned)(s_idx + 1), (unsigned)scenario_count());
}

/* ------------------------------------------------------------------ botón */

static void on_short_press(void) {
    if (!s_active) return; /* fuera de demo el BOOT no hace nada */
    demo_next();
}

static void on_long_press(void) {
    if (s_active)
        demo_exit();
    else
        demo_enter();
    sound_play_test(); /* pitido corto de confirmación */
}

/* -------------------------------------------------------------------- API */

void demo_init(void) {
    pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
    s_enabled = true;
    s_raw_last = false;
    s_pressed = false;
    Serial.printf("[demo] boton BOOT listo (larga %d ms = demo on/off)\n",
                  DEMO_HOLD_MS);
}

void demo_tick(void) {
    if (!s_enabled) return;
    uint32_t now = millis();

    bool raw = digitalRead(BOOT_BTN_PIN) == LOW; /* activo a nivel bajo */
    if (raw != s_raw_last) {
        s_raw_last = raw;
        s_raw_since = now;
    }
    if (raw != s_pressed && now - s_raw_since >= DEMO_DEBOUNCE_MS) {
        s_pressed = raw;
        if (s_pressed) {
            s_press_start = now;
            s_long_done = false;
        } else if (!s_long_done) {
            on_short_press(); /* soltado antes del umbral = corta */
        }
    }
    /* la larga salta con el botón aún pulsado: se nota sin soltar */
    if (s_pressed && !s_long_done && now - s_press_start >= DEMO_HOLD_MS) {
        s_long_done = true;
        on_long_press();
    }

    if (s_active && now - s_last_force >= 1000) {
        s_last_force = now;
        force_conn();
    }
}

bool demo_is_active(void) { return s_active; }

uint32_t demo_seq(void) { return s_seq; }
