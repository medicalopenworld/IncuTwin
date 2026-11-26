#include "demo_mode.h"

#include <stdio.h>

#include "incupray.h"

#include "board.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "scenarios.h"
#include "sound.h"
#include "storage.h"
#include "telemetry.h"
#include "twin_model.h"

static const char *TAG = "demo";

#define SAMPLE_MS   10
#define DEBOUNCE_MS 30
#define HOLD_MS     2000

static size_t s_idx;
static bool s_pressed;      /* nivel estable */
static bool s_raw_last;
static uint32_t s_raw_since_ms;
static uint32_t s_press_start_ms;
static bool s_long_done;

/* Entra o sale de la demo y lo registra: dentro de la demo telemetry solo deja pasar `demo`.
 * Entrar: set_demo(true) y luego `on`; salir: `off` (con la duracion) y luego set_demo(false). */
static int64_t s_on_us;

void demo_mode_set(bool on)
{
    if (on == twin_model_demo_active()) return;
    if (on) {
        twin_model_demo_enter();
        s_on_us = esp_timer_get_time();
        telemetry_set_demo(true);
        telemetry_log("demo", "{\"ev\":\"on\"}");
    } else {
        char d[40];
        snprintf(d, sizeof d, "{\"ev\":\"off\",\"dur_s\":%lu}",
                 (unsigned long)((esp_timer_get_time() - s_on_us) / 1000000));
        telemetry_log("demo", d);
        telemetry_set_demo(false);
        incupray_demo_stop();
        twin_model_demo_exit();
    }
}

/* Restablecimiento de fabrica con telemetria: sale antes de la demo (si no, telemetry_log solo deja
 * pasar `demo` y el `reset` se perderia), registra `reset` con el gen actual, guarda y envia lo
 * pendiente (espera < 2 s) y borra. Lo usan la UI (pulsacion larga en Ajustes) y la consola. */
void demo_mode_factory_reset(void)
{
    demo_mode_set(false);
    char d[40];
    snprintf(d, sizeof d, "{\"gen\":%lu}", (unsigned long)telemetry_gen());
    telemetry_log("reset", d);
    telemetry_flush_now(2000);
    storage_factory_reset();
}

static void apply_idx(size_t idx)
{
    const twin_scenario_t *sc = scenario_get(idx);
    if (!sc) return;
    twin_model_demo_apply(&sc->inc, sc->linked);
    ESP_LOGI(TAG, "escenario '%s' (%u/%u)", sc->id, (unsigned)(idx + 1), (unsigned)scenario_count());
}

/* Tras el ultimo escenario, un paso mas: la IncuPray simulada (3 min) sobre ese mismo escenario */
static void on_short(void)
{
    if (!twin_model_demo_active()) return;
    s_idx = (s_idx + 1) % (scenario_count() + 1);
    if (s_idx == scenario_count()) {
        incupray_demo_start();
        ESP_LOGI(TAG, "IncuPray simulada");
        return;
    }
    incupray_demo_stop();
    apply_idx(s_idx);
}

static void on_long(void)
{
    if (twin_model_demo_active()) {
        demo_mode_set(false);
        ESP_LOGI(TAG, "OFF");
    } else {
        demo_mode_set(true);
        s_idx = 0;
        apply_idx(s_idx);
        ESP_LOGI(TAG, "ON");
    }
    sound_request(SOUND_TEST);
}

static void sample_cb(void *arg)
{
    (void)arg;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    bool raw = board_button_pressed();
    if (raw != s_raw_last) {
        s_raw_last = raw;
        s_raw_since_ms = now;
    }
    if (raw != s_pressed && now - s_raw_since_ms >= DEBOUNCE_MS) {
        s_pressed = raw;
        if (s_pressed) {
            s_press_start_ms = now;
            s_long_done = false;
        } else if (!s_long_done) {
            on_short(); /* soltado antes del umbral */
        }
    }
    if (s_pressed && !s_long_done && now - s_press_start_ms >= HOLD_MS) {
        s_long_done = true; /* la larga salta con el boton aun pulsado */
        on_long();
    }
}

void demo_mode_start(void)
{
    const esp_timer_create_args_t args = {
        .callback = sample_cb,
        .name = "demo_btn",
        .dispatch_method = ESP_TIMER_TASK,
    };
    esp_timer_handle_t t;
    ESP_ERROR_CHECK(esp_timer_create(&args, &t));
    ESP_ERROR_CHECK(esp_timer_start_periodic(t, SAMPLE_MS * 1000));
    ESP_LOGI(TAG, "boton BOOT listo (larga %d ms = demo on/off)", HOLD_MS);
}
