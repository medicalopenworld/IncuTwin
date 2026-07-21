#include "app_state.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"

twin_state_t g_state;
volatile bool g_state_dirty = false;

static SemaphoreHandle_t s_mutex;

void state_init(void) {
    s_mutex = xSemaphoreCreateMutex();
    g_state = {};
    g_state.thermo = THERMO_OFF;
    g_state.skin_tone = DEFAULT_SKIN_TONE;
    g_state.awake = false;
    g_state.baby_present = true; /* bridges without "baby" keep working */
    g_state_dirty = true;
}

void state_lock(void) { xSemaphoreTake(s_mutex, portMAX_DELAY); }
void state_unlock(void) { xSemaphoreGive(s_mutex); }

thermo_state_t thermo_from_str(const char *s) {
    if (!s) return THERMO_OFF;
    if (!strcmp(s, "heating")) return THERMO_HEATING;
    if (!strcmp(s, "stable")) return THERMO_STABLE;
    if (!strcmp(s, "alarm")) return THERMO_ALARM;
    return THERMO_OFF;
}
