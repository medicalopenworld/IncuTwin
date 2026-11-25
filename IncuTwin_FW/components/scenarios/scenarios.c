#include "scenarios.h"

#include <string.h>

#define INC(_online, _baby, _thermo, _photo, _bpm, _awake, _sex)                             \
    {                                                                                         \
        .online = _online, .baby = _baby, .thermo = _thermo, .photo = _photo,                 \
        .pulseox = (_bpm) > 0, .bpm = _bpm, .awake = _awake, .skin = 1, .name = "",           \
        .country = "CM", .sex = _sex, .weight_g = 0, .age_d = -1 /* demo: Camerun */          \
    }
#define M TWIN_SEX_MALE
#define F TWIN_SEX_FEMALE

/* Orden = ciclado con BOOT: se abre con el bebe tranquilo y se cierra con los
 * estados "sin datos". Alterna nino y nina para ensenar los dos dibujos. */
static const twin_scenario_t SCENARIOS[] = {
    { "sleep",    true,  INC(true,  TWIN_BABY_IN,      TWIN_THERMO_STABLE,  false, 120, false, M) },
    { "awake",    true,  INC(true,  TWIN_BABY_IN,      TWIN_THERMO_STABLE,  false, 140, true, F) },
    { "heating",  true,  INC(true,  TWIN_BABY_IN,      TWIN_THERMO_HEATING, false, 130, false, M) },
    { "photo",    true,  INC(true,  TWIN_BABY_IN,      TWIN_THERMO_STABLE,  true,  130, false, F) },
    { "parents",  true,  INC(true,  TWIN_BABY_PARENTS, TWIN_THERMO_OFF,     false, 0,   false, M) },
    { "home",     true,  INC(true,  TWIN_BABY_HOME,    TWIN_THERMO_OFF,     false, 0,   false, F) },
    { "empty",    true,  INC(true,  TWIN_BABY_NONE,    TWIN_THERMO_STABLE,  false, 0,   false, M) },
    { "off",      true,  INC(false, TWIN_BABY_IN,      TWIN_THERMO_OFF,     false, 0,   false, M) },
    { "unlinked", false, INC(false, TWIN_BABY_NONE,    TWIN_THERMO_OFF,     false, 0,   false, M) },
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
