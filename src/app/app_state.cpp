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
    g_state.baby = BABY_IN; /* bridges without "baby" keep working */
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

baby_state_t baby_from_str(const char *s) {
    if (!s) return BABY_NONE;
    if (!strcmp(s, "in")) return BABY_IN;
    if (!strcmp(s, "parents")) return BABY_PARENTS;
    if (!strcmp(s, "out")) return BABY_OUT;
    return BABY_NONE;
}

const char *baby_to_str(baby_state_t b) {
    switch (b) {
        case BABY_IN: return "in";
        case BABY_PARENTS: return "parents";
        case BABY_OUT: return "out";
        default: return "none";
    }
}
