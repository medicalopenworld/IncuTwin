#include "twin_fields.h"

#include <Arduino.h>
#include <string.h>

#include "app/app_state.h"

bool twin_apply_field(const char *key, JsonVariantConst v) {
    if (!strcmp(key, "online")) {
        g_state.incubator_online = v.as<bool>();
    } else if (!strcmp(key, "thermo")) {
        g_state.thermo = thermo_from_str(v.as<const char *>());
    } else if (!strcmp(key, "photo")) {
        g_state.phototherapy = v.as<bool>();
    } else if (!strcmp(key, "hr")) {
        g_state.heart_rate = (uint16_t)constrain(v.as<int>(), 0, 300);
    } else if (!strcmp(key, "skin")) {
        int t = v.as<int>();
        if (t >= 0 && t < 6) g_state.skin_tone = (uint8_t)t;
    } else if (!strcmp(key, "awake")) {
        g_state.awake = v.as<bool>();
    } else if (!strcmp(key, "baby")) {
        /* compatibilidad con el campo bool anterior: true=in, false=none */
        if (v.is<bool>())
            g_state.baby = v.as<bool>() ? BABY_IN : BABY_NONE;
        else
            g_state.baby = baby_from_str(v.as<const char *>());
    } else if (!strcmp(key, "home")) {
        g_state.home = v.as<bool>();
    } else if (!strcmp(key, "weight_g")) {
        g_state.baby_weight_g = (uint16_t)constrain(v.as<int>(), 0, 9999);
    } else if (!strcmp(key, "age_d")) {
        g_state.baby_age_d = (int16_t)constrain(v.as<int>(), -1, 9999);
    } else if (!strcmp(key, "name")) {
        strlcpy(g_state.baby_name, v.as<const char *>() ? v.as<const char *>() : "",
                sizeof(g_state.baby_name));
    } else {
        return false;
    }
    return true;
}
