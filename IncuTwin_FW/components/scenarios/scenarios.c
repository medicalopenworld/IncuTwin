#include "scenarios.h"

#include <string.h>

#define INC(_online, _baby, _thermo, _photo, _bpm, _awake)                                   \
    {                                                                                         \
        .online = _online, .baby = _baby, .thermo = _thermo, .photo = _photo,                 \
        .pulseox = (_bpm) > 0, .bpm = _bpm, .awake = _awake, .skin = 1, .name = "",           \
        .weight_g = 0, .age_d = -1                                                            \
    }

/* Orden = ciclado con BOOT: se abre con el bebe tranquilo y se cierra con los
 * estados "sin datos". */
static const twin_scenario_t SCENARIOS[] = {
    { "sleep",    true,  INC(true,  TWIN_BABY_IN,      TWIN_THERMO_STABLE,  false, 120, false) },
    { "awake",    true,  INC(true,  TWIN_BABY_IN,      TWIN_THERMO_STABLE,  false, 140, true) },
    { "heating",  true,  INC(true,  TWIN_BABY_IN,      TWIN_THERMO_HEATING, false, 130, false) },
    { "alarm",    true,  INC(true,  TWIN_BABY_IN,      TWIN_THERMO_ALARM,   false, 180, true) },
    { "photo",    true,  INC(true,  TWIN_BABY_IN,      TWIN_THERMO_STABLE,  true,  130, false) },
    { "parents",  true,  INC(true,  TWIN_BABY_PARENTS, TWIN_THERMO_OFF,     false, 0,   false) },
    { "home",     true,  INC(true,  TWIN_BABY_HOME,    TWIN_THERMO_OFF,     false, 0,   false) },
    { "empty",    true,  INC(true,  TWIN_BABY_NONE,    TWIN_THERMO_STABLE,  false, 0,   false) },
    { "off",      true,  INC(false, TWIN_BABY_IN,      TWIN_THERMO_OFF,     false, 0,   false) },
    { "unlinked", false, INC(false, TWIN_BABY_NONE,    TWIN_THERMO_OFF,     false, 0,   false) },
};

#define N (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

size_t scenario_count(void) { return N; }

const twin_scenario_t *scenario_get(size_t idx) { return idx < N ? &SCENARIOS[idx] : NULL; }

const twin_scenario_t *scenario_find(const char *id)
{
    if (!id) return NULL;
    for (size_t i = 0; i < N; i++) {
        if (strcmp(SCENARIOS[i].id, id) == 0) return &SCENARIOS[i];
    }
    return NULL;
}
