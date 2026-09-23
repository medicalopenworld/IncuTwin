#include "scenarios.h"

#include <Arduino.h>
#include <string.h>

/* Orden = orden del ciclado con el botón BOOT y de la demo automática
 * del panel web: se abre con el bebé tranquilo y se cierra con los
 * estados "sin datos". */
static const scenario_t SCENARIOS[] = {
    {"sleep",    true,  true,  THERMO_STABLE,  false, 120, false, true},
    {"awake",    true,  true,  THERMO_STABLE,  false, 140, true,  true},
    {"heating",  true,  true,  THERMO_HEATING, false, 130, false, true},
    {"alarm",    true,  true,  THERMO_ALARM,   false, 180, true,  true},
    {"photo",    true,  true,  THERMO_STABLE,  true,  130, false, true},
    {"empty",    true,  true,  THERMO_STABLE,  false, 0,   false, false},
    {"off",      true,  false, THERMO_OFF,     false, 0,   false, true},
    {"unlinked", false, false, THERMO_OFF,     false, 0,   false, true},
};
static const size_t N_SCENARIOS = sizeof(SCENARIOS) / sizeof(SCENARIOS[0]);

size_t scenario_count(void) { return N_SCENARIOS; }

const char *scenario_id(size_t idx) {
    return idx < N_SCENARIOS ? SCENARIOS[idx].id : NULL;
}

void scenario_apply_idx(size_t idx) {
    if (idx >= N_SCENARIOS) return;
    const scenario_t *sc = &SCENARIOS[idx];
    Serial.printf("[scenario] '%s' t=%lu\n", sc->id, (unsigned long)millis());
    state_lock();
    g_state.node_seen = sc->linked;
    g_state.incubator_online = sc->online;
    g_state.thermo = sc->thermo;
    g_state.phototherapy = sc->photo;
    g_state.heart_rate = sc->hr;
    g_state.awake = sc->awake;
    g_state.baby_present = sc->baby;
    g_state.last_update_ms = millis();
    g_state_dirty = true;
    state_unlock();
}

bool scenario_apply(const char *id) {
    if (!id) return false;
    for (size_t i = 0; i < N_SCENARIOS; i++) {
        if (strcmp(SCENARIOS[i].id, id) != 0) continue;
        scenario_apply_idx(i);
        return true;
    }
    return false;
}
