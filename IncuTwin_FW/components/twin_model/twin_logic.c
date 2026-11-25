#include "twin_logic.h"

#include "twin_geo.h"

#include <string.h>

void twin_incubator_reset(twin_incubator_t *inc, uint8_t default_skin)
{
    memset(inc, 0, sizeof(*inc));
    inc->baby = TWIN_BABY_NONE;
    inc->thermo = TWIN_THERMO_OFF;
    inc->skin = default_skin < TWIN_SKIN_COUNT ? default_skin : 0;
    inc->age_d = -1;
}

twin_apply_result_t twin_apply(twin_incubator_t *inc, uint32_t *last_seq, const twin_msg_t *m)
{
    twin_apply_result_t r = { 0 };
    r.is_new = !m->has_seq || m->seq > *last_seq;
    twin_baby_t before = inc->baby;

    /* mapeo base (estado completo: lo ausente se apaga) */
    inc->online = m->state != TWIN_MSG_STATE_OFFLINE;
    /* un "offline" sin baby da none; ThingsBoard siempre manda baby (itw_state_machine.js), asi que persistirlo es seguro */
    inc->baby = m->state == TWIN_MSG_STATE_BABY ? TWIN_BABY_IN : TWIN_BABY_NONE;
    inc->thermo = m->heat ? TWIN_THERMO_STABLE : TWIN_THERMO_OFF;
    inc->photo = m->photo;
    inc->pulseox = m->pulseox;
    inc->bpm = (m->pulseox && m->bpm >= TWIN_BPM_SHOW_MIN && m->bpm <= TWIN_BPM_SHOW_MAX) ? (uint16_t)m->bpm : 0;

    /* extensiones: prevalecen si vienen; name/sex/weight/age se borran si no vienen
     * (ThingsBoard publica el estado completo cada vez) */
    if (m->has_thermo) inc->thermo = m->thermo;
    if (m->has_baby) inc->baby = m->baby;
    inc->awake = m->has_awake ? m->awake : false;
    if (m->has_name) {
        strncpy(inc->name, m->name, TWIN_NAME_LEN - 1);
        inc->name[TWIN_NAME_LEN - 1] = '\0';
    } else {
        inc->name[0] = '\0';
    }
    if (m->has_country) {
        memcpy(inc->country, m->country, TWIN_COUNTRY_LEN);
    } else {
        inc->country[0] = '\0';
    }
    /* tono: el explicito manda y se conserva; si no, el del pais (4 de 5 si no se sabe) */
    if (m->has_skin) {
        inc->skin = m->skin;
        inc->skin_set = true;
    } else if (!inc->skin_set) {
        inc->skin = twin_geo_skin(inc->country);
    }
    inc->sex = m->has_sex ? m->sex : TWIN_SEX_UNKNOWN;
    if (m->has_stay) memcpy(inc->stay_id, m->stay_id, TWIN_STAY_LEN);
    else inc->stay_id[0] = '\0';
    inc->weight_g = m->has_weight ? m->weight_g : 0;
    inc->age_d = m->has_age ? m->age_d : -1;

    if (r.is_new) {
        if (m->has_seq) {
            *last_seq = m->seq;
        }
        bool heartbeat = strcmp(m->last_event, "heartbeat") == 0;
        r.persist = (m->has_seq && !heartbeat) || inc->baby != before;
    }
    return r;
}

void twin_derive(twin_snapshot_t *s)
{
    const twin_link_t *l = &s->link;
    s->link_ok = l->wifi && l->has_creds && l->broker_once && !l->broker_lost && l->paired &&
                 l->state_rx && s->inc.online;
    s->show_baby = s->link_ok && s->inc.baby == TWIN_BABY_IN;
    s->parents_view = s->link_ok &&
                      (s->inc.baby == TWIN_BABY_PARENTS || s->inc.baby == TWIN_BABY_HOME);
}

twin_status_t twin_status_of(const twin_snapshot_t *s)
{
    const twin_link_t *l = &s->link;
    bool acc = s->acc != TWIN_ACC_OFF;
    if (s->parents_view && s->inc.baby == TWIN_BABY_HOME)
        return s->acc == TWIN_ACC_FAREWELL ? TWIN_ST_ACC_FAREWELL : TWIN_ST_HOME;
    if (s->parents_view) return TWIN_ST_PARENTS;
    if (!l->wifi) return TWIN_ST_NO_WIFI;
    if (!l->has_creds) return TWIN_ST_NO_CREDS;
    if (!l->broker_once) return TWIN_ST_CONNECTING;
    if (l->broker_lost) return TWIN_ST_BROKER_LOST;
    if (!l->paired) return acc ? TWIN_ST_ACC_SEARCH : TWIN_ST_UNPAIRED;
    if (!l->state_rx) return TWIN_ST_WAITING;
    if (!s->inc.online) return TWIN_ST_INC_OFF;
    if (s->inc.baby != TWIN_BABY_IN) return s->acc == TWIN_ACC_ENDING ? TWIN_ST_ACC_ENDED : TWIN_ST_NO_BABY;
    if (s->acc == TWIN_ACC_WAITING) return TWIN_ST_ACC_WAITING;
    if (acc && s->acc_remind) return TWIN_ST_ACC_REMIND;
    if (s->inc.name[0]) return TWIN_ST_NAME;
    return s->inc.awake ? TWIN_ST_AWAKE : TWIN_ST_SLEEP;
}

uint8_t twin_rssi_level(int8_t rssi)
{
    if (rssi >= -55) return 3;
    if (rssi >= -67) return 2;
    if (rssi >= -78) return 1;
    return 0;
}

bool twin_incubator_id_valid(const char *id)
{
    if (!id || !id[0]) return false;
    size_t n = 0;
    for (const char *p = id; *p; p++, n++) {
        char c = *p;
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '-';
        if (!ok || n >= TWIN_INCUBATOR_ID_LEN - 1) return false;
    }
    return true;
}
