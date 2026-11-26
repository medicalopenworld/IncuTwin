#include "incupray_logic.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"

ip_phase_t ip_phase(const ip_window_t *w, uint32_t now)
{
    if (!w || !now || !w->start || now < w->start) return IP_BEFORE;
    return now < w->start + w->duration_s ? IP_OPEN : IP_AFTER;
}

uint32_t ip_remaining_s(const ip_window_t *w, uint32_t now)
{
    return ip_phase(w, now) == IP_OPEN ? w->start + w->duration_s - now : 0;
}

ip_notice_t ip_notice_due(const ip_window_t *w, uint32_t now, uint8_t shown)
{
    if (!w || !now || !w->start) return IP_NOTICE_NONE;
    ip_notice_t n = IP_NOTICE_NONE;
    if (ip_phase(w, now) == IP_OPEN) {
        n = IP_NOTICE_START;
    } else if (now < w->start && now + IP_HOUR_S >= w->start) {
        n = IP_NOTICE_HOUR;
    } else if (now < w->start && now + IP_EVE_S >= w->start) {
        n = IP_NOTICE_EVE;
    }
    return (shown & n) ? IP_NOTICE_NONE : n;
}

uint16_t ip_stake(uint16_t balance) { return balance >= IP_STAKE ? IP_STAKE : 0; }

uint8_t ip_hearts_for(uint32_t held_s)
{
    uint32_t h = held_s / IP_SECONDS_PER_HEART;
    return (uint8_t)(h > IP_HEARTS_MAX ? IP_HEARTS_MAX : h);
}

/* ------------------------------------------------------------- mensajes */

static bool copy_str(const cJSON *o, const char *key, char *dst, size_t n)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsString(v) || !v->valuestring || strlen(v->valuestring) >= n) return false;
    strcpy(dst, v->valuestring);
    return true;
}

static bool get_num(const cJSON *o, const char *key, double *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsNumber(v) || v->valuedouble < 0) return false;
    *out = v->valuedouble;
    return true;
}

bool ip_parse_next(const char *payload, size_t len, ip_window_t *out)
{
    memset(out, 0, sizeof(*out));
    cJSON *r = cJSON_ParseWithLength(payload, len);
    if (!r) return false;
    double start = 0, dur = 0;
    bool ok = cJSON_IsObject(r) && copy_str(r, "id", out->id, sizeof(out->id)) && out->id[0] &&
              get_num(r, "start", &start) && get_num(r, "duration_s", &dur) && start > 0 && dur > 0 &&
              dur <= 24 * 3600;
    if (ok) {
        out->start = (uint32_t)start;
        out->duration_s = (uint32_t)dur;
    }
    cJSON_Delete(r);
    return ok;
}

bool ip_parse_live(const char *payload, size_t len, ip_live_t *out)
{
    memset(out, 0, sizeof(*out));
    cJSON *r = cJSON_ParseWithLength(payload, len);
    if (!r) return false;
    double count = 0, hearts = 0;
    bool ok = cJSON_IsObject(r) && copy_str(r, "id", out->id, sizeof(out->id)) && get_num(r, "count", &count);
    if (ok) {
        out->count = (uint16_t)(count > 65535 ? 65535 : count);
        if (get_num(r, "hearts", &hearts)) out->hearts = (uint32_t)hearts;
        const cJSON *names = cJSON_GetObjectItemCaseSensitive(r, "names");
        const cJSON *it;
        cJSON_ArrayForEach(it, names) {
            if (out->n_names >= IP_NAMES_MAX) break;
            if (!cJSON_IsString(it) || !it->valuestring || strlen(it->valuestring) >= IP_NAME_BYTES) continue;
            strcpy(out->names[out->n_names++], it->valuestring);
        }
        const cJSON *babies = cJSON_GetObjectItemCaseSensitive(r, "babies");
        cJSON_ArrayForEach(it, babies) {
            if (out->n_babies >= IP_BABIES_MAX) break;
            ip_baby_t *b = &out->babies[out->n_babies];
            memset(b, 0, sizeof(*b));
            if (!copy_str(it, "country", b->country, sizeof(b->country)) || strlen(b->country) != 2) continue;
            copy_str(it, "inc", b->inc, sizeof(b->inc));
            char sex[4] = "";
            copy_str(it, "sex", sex, sizeof(sex));
            b->sex = sex[0] == 'f' ? TWIN_SEX_FEMALE : sex[0] == 'm' ? TWIN_SEX_MALE : TWIN_SEX_UNKNOWN;
            double skin = 0;
            if (get_num(it, "skin", &skin) && skin <= 4) b->skin = (uint8_t)skin;
            out->n_babies++;
        }
    }
    cJSON_Delete(r);
    return ok;
}

bool ip_parse_summary(const char *payload, size_t len, ip_summary_t *out)
{
    memset(out, 0, sizeof(*out));
    cJSON *r = cJSON_ParseWithLength(payload, len);
    if (!r) return false;
    double count = 0, hearts = 0;
    bool ok = cJSON_IsObject(r) && copy_str(r, "id", out->id, sizeof(out->id)) && get_num(r, "count", &count) &&
              get_num(r, "hearts", &hearts);
    if (ok) {
        out->count = (uint16_t)(count > 65535 ? 65535 : count);
        out->hearts = (uint32_t)hearts;
    }
    cJSON_Delete(r);
    return ok;
}

int ip_participation_build(char *out, size_t n, const char *id, uint32_t held_s, uint8_t hearts)
{
    int k = snprintf(out, n, "{\"id\":\"%s\",\"joined\":true,\"held_s\":%lu,\"hearts\":%u}", id ? id : "",
                     (unsigned long)held_s, hearts);
    return (k < 0 || (size_t)k >= n) ? -1 : k;
}
