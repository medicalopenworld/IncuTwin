#include "commands_parse.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "twin_logic.h"

static bool take_name(const char *rest, char name[CMD_NAME_MAX])
{
    size_t n = strlen(rest);
    if (n == 0 || n >= CMD_NAME_MAX || strchr(rest, '/')) return false;
    strcpy(name, rest);
    return true;
}

cmd_topic_kind_t cmd_parse_topic(const char *topic, const char *client_id, char name[CMD_NAME_MAX])
{
    name[0] = '\0';
    if (!topic) return CMD_TOPIC_NONE;
    char prefix[96];
    snprintf(prefix, sizeof(prefix), "incutwin/%s/cmd/", client_id ? client_id : "");
    size_t pl = strlen(prefix);
    if (client_id && client_id[0] && strncmp(topic, prefix, pl) == 0) {
        return take_name(topic + pl, name) ? CMD_TOPIC_UNICAST : CMD_TOPIC_NONE;
    }
    const char fleet[] = "incutwin/all/cmd/";
    if (strncmp(topic, fleet, sizeof(fleet) - 1) == 0) {
        return take_name(topic + sizeof(fleet) - 1, name) ? CMD_TOPIC_FLEET : CMD_TOPIC_NONE;
    }
    return CMD_TOPIC_NONE;
}

bool cmd_is_state_topic(const char *topic, char out_id[TWIN_INCUBATOR_ID_LEN])
{
    const char pre[] = "incubators/";
    const char suf[] = "/state";
    if (!topic || strncmp(topic, pre, sizeof(pre) - 1) != 0) return false;
    const char *id = topic + sizeof(pre) - 1;
    const char *end = strstr(id, suf);
    if (!end || end[sizeof(suf) - 1] != '\0') return false;
    size_t n = (size_t)(end - id);
    if (n == 0 || n >= TWIN_INCUBATOR_ID_LEN) return false;
    char tmp[TWIN_INCUBATOR_ID_LEN];
    memcpy(tmp, id, n);
    tmp[n] = '\0';
    if (!twin_incubator_id_valid(tmp)) return false;
    if (out_id) strcpy(out_id, tmp);
    return true;
}

pair_cmd_t cmd_parse_pair(const char *payload, size_t len, char out_id[TWIN_INCUBATOR_ID_LEN])
{
    out_id[0] = '\0';
    if (len == 0) return PAIR_UNPAIR;
    cJSON *root = cJSON_ParseWithLength(payload, len);
    if (!root) return PAIR_INVALID;
    pair_cmd_t res = PAIR_INVALID;
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "incubator_id");
    if (cJSON_IsObject(root) && cJSON_IsString(id) && id->valuestring &&
        twin_incubator_id_valid(id->valuestring)) {
        strncpy(out_id, id->valuestring, TWIN_INCUBATOR_ID_LEN - 1);
        out_id[TWIN_INCUBATOR_ID_LEN - 1] = '\0';
        res = PAIR_SET;
    }
    cJSON_Delete(root);
    return res;
}

bool cmd_parse_brightness(const char *payload, size_t len, int *out)
{
    cJSON *root = cJSON_ParseWithLength(payload, len);
    if (!root) return false;
    bool ok = false;
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "value");
    if (cJSON_IsNumber(v)) {
        int n = (int)v->valuedouble;
        *out = n < 10 ? 10 : n > 100 ? 100 : n;
        ok = true;
    }
    cJSON_Delete(root);
    return ok;
}

void cmd_parse_melody(const char *payload, size_t len, char out[16])
{
    out[0] = '\0';
    if (len == 0) return;
    cJSON *root = cJSON_ParseWithLength(payload, len);
    if (!root) return;
    const cJSON *m = cJSON_GetObjectItemCaseSensitive(root, "melody");
    if (cJSON_IsString(m) && m->valuestring) {
        static const char *const VALID[] = { "boot", "baby", "parents", "test" };
        for (size_t i = 0; i < 4; i++) {
            if (strcmp(m->valuestring, VALID[i]) == 0) {
                strcpy(out, VALID[i]);
                break;
            }
        }
    }
    cJSON_Delete(root);
}
