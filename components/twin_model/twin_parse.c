#include <string.h>

#include "cJSON.h"
#include "twin_logic.h"

static bool str_eq(const cJSON *item, const char *s)
{
    return cJSON_IsString(item) && item->valuestring && strcmp(item->valuestring, s) == 0;
}

static bool get_int(const cJSON *item, long min, long max, long *out)
{
    if (!cJSON_IsNumber(item)) return false;
    double v = item->valuedouble;
    if (v < (double)min || v > (double)max || v != (double)(long)v) return false;
    *out = (long)v;
    return true;
}

static void parse_treatments(const cJSON *arr, twin_msg_t *m)
{
    if (!cJSON_IsArray(arr)) return;
    const cJSON *it;
    cJSON_ArrayForEach(it, arr) {
        if (str_eq(it, "heat")) m->heat = true;
        else if (str_eq(it, "phototherapy")) m->photo = true;
        else if (str_eq(it, "pulseox")) m->pulseox = true;
    }
}

static void parse_extensions(const cJSON *root, twin_msg_t *m)
{
    const cJSON *it;
    long v;

    it = cJSON_GetObjectItemCaseSensitive(root, "thermo");
    if (str_eq(it, "off")) { m->has_thermo = true; m->thermo = TWIN_THERMO_OFF; }
    else if (str_eq(it, "heating")) { m->has_thermo = true; m->thermo = TWIN_THERMO_HEATING; }
    else if (str_eq(it, "stable")) { m->has_thermo = true; m->thermo = TWIN_THERMO_STABLE; }
    else if (str_eq(it, "alarm")) { m->has_thermo = true; m->thermo = TWIN_THERMO_ALARM; }

    it = cJSON_GetObjectItemCaseSensitive(root, "baby");
    if (str_eq(it, "none")) { m->has_baby = true; m->baby = TWIN_BABY_NONE; }
    else if (str_eq(it, "in")) { m->has_baby = true; m->baby = TWIN_BABY_IN; }
    else if (str_eq(it, "parents")) { m->has_baby = true; m->baby = TWIN_BABY_PARENTS; }
    else if (str_eq(it, "home")) { m->has_baby = true; m->baby = TWIN_BABY_HOME; }

    it = cJSON_GetObjectItemCaseSensitive(root, "awake");
    if (cJSON_IsBool(it)) { m->has_awake = true; m->awake = cJSON_IsTrue(it); }

    it = cJSON_GetObjectItemCaseSensitive(root, "skin");
    if (get_int(it, 0, TWIN_SKIN_COUNT - 1, &v)) { m->has_skin = true; m->skin = (uint8_t)v; }

    it = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (cJSON_IsString(it) && it->valuestring && strlen(it->valuestring) < TWIN_NAME_LEN) {
        m->has_name = it->valuestring[0] != '\0';
        strncpy(m->name, it->valuestring, TWIN_NAME_LEN - 1);
    }

    it = cJSON_GetObjectItemCaseSensitive(root, "weight_g");
    if (get_int(it, 0, 9999, &v)) { m->has_weight = v > 0; m->weight_g = (uint16_t)v; }

    it = cJSON_GetObjectItemCaseSensitive(root, "age_d");
    if (get_int(it, 0, 9999, &v)) { m->has_age = true; m->age_d = (int16_t)v; }
}

twin_parse_result_t twin_parse(const char *json, size_t len, const char *expected_id,
                               twin_msg_t *out)
{
    memset(out, 0, sizeof(*out));
    out->bpm = -1;
    if (len > TWIN_PARSE_MAX_LEN) return TWIN_PARSE_TOO_BIG;

    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root) return TWIN_PARSE_BAD_JSON;
    twin_parse_result_t res = TWIN_PARSE_OK;

    if (!cJSON_IsObject(root)) {
        res = TWIN_PARSE_NOT_OBJECT;
        goto out;
    }

    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "incubator_id");
    if (!cJSON_IsString(id) || !id->valuestring || !twin_incubator_id_valid(id->valuestring) ||
        (expected_id && expected_id[0] && strcmp(id->valuestring, expected_id) != 0)) {
        res = TWIN_PARSE_BAD_ID;
        goto out;
    }
    strncpy(out->incubator_id, id->valuestring, TWIN_INCUBATOR_ID_LEN - 1);

    const cJSON *st = cJSON_GetObjectItemCaseSensitive(root, "state");
    if (str_eq(st, "free")) out->state = TWIN_MSG_STATE_FREE;
    else if (str_eq(st, "baby")) out->state = TWIN_MSG_STATE_BABY;
    else if (str_eq(st, "offline")) out->state = TWIN_MSG_STATE_OFFLINE;
    else {
        res = TWIN_PARSE_BAD_STATE;
        goto out;
    }

    parse_treatments(cJSON_GetObjectItemCaseSensitive(root, "treatments"), out);

    long v;
    if (get_int(cJSON_GetObjectItemCaseSensitive(root, "bpm"), 0, 100000, &v)) out->bpm = (int)v;
    if (get_int(cJSON_GetObjectItemCaseSensitive(root, "event_seq"), 0, 0x7fffffffL, &v)) {
        out->has_seq = true;
        out->seq = (uint32_t)v;
    }
    const cJSON *ev = cJSON_GetObjectItemCaseSensitive(root, "last_event");
    if (cJSON_IsString(ev) && ev->valuestring) {
        strncpy(out->last_event, ev->valuestring, sizeof(out->last_event) - 1);
    }
    parse_extensions(root, out);

out:
    cJSON_Delete(root);
    return res;
}

const char *twin_parse_result_str(twin_parse_result_t r)
{
    switch (r) {
    case TWIN_PARSE_OK:         return "ok";
    case TWIN_PARSE_TOO_BIG:    return "payload > 1024 B";
    case TWIN_PARSE_BAD_JSON:   return "json invalido";
    case TWIN_PARSE_NOT_OBJECT: return "no es un objeto";
    case TWIN_PARSE_BAD_ID:     return "incubator_id ausente o distinto";
    case TWIN_PARSE_BAD_STATE:  return "state ausente o desconocido";
    default:                    return "?";
    }
}
