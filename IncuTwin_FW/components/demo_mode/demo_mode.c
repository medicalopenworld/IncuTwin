#include "demo_mode.h"

#include "board.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "scenarios.h"
#include "sound.h"
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

static void apply_idx(size_t idx)
{
    const twin_scenario_t *sc = scenario_get(idx);
    if (!sc) return;
    twin_model_demo_apply(&sc->inc, sc->linked);
    ESP_LOGI(TAG, "escenario '%s' (%u/%u)", sc->id, (unsigned)(idx + 1), (unsigned)scenario_count());
}

static void on_short(void)
{
    if (!twin_model_demo_active()) return;
    s_idx = (s_idx + 1) % scenario_count();
    apply_idx(s_idx);
}

static void on_long(void)
{
    if (twin_model_demo_active()) {
        twin_model_demo_exit();
        ESP_LOGI(TAG, "OFF");
    } else {
        twin_model_demo_enter();
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
