#include "ota_validate.h"

#include <ctype.h>
#include <string.h>

#include "cJSON.h"

bool ota_url_allowed(const char *url, const char *host_suffix, bool allow_any_host)
{
    const char scheme[] = "https://";
    if (!url || strncmp(url, scheme, sizeof(scheme) - 1) != 0) return false;
    const char *host = url + sizeof(scheme) - 1;
    size_t hlen = strcspn(host, "/:?#");
    if (hlen == 0) return false;
    if (allow_any_host) return true;
    if (!host_suffix || !host_suffix[0]) return false;
    size_t slen = strlen(host_suffix);
    if (hlen < slen + 1) return false; /* al menos un caracter antes del sufijo */
    for (size_t i = 0; i < slen; i++) {
        if (tolower((unsigned char)host[hlen - slen + i]) != tolower((unsigned char)host_suffix[i])) {
            return false;
        }
    }
    return true;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool ota_parse_sha256(const char *hex, uint8_t out[32])
{
    if (!hex || strlen(hex) != 64) return false;
    for (int i = 0; i < 32; i++) {
        int hi = hexval(hex[2 * i]), lo = hexval(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

ota_cmd_result_t ota_validate_cmd(const char *payload, size_t len, const char *host_suffix,
                                  bool allow_any_host, ota_job_t *out)
{
    memset(out, 0, sizeof(*out));
    cJSON *root = cJSON_ParseWithLength(payload, len);
    if (!root) return OTA_CMD_BAD_JSON;
    ota_cmd_result_t res = OTA_CMD_OK;
    const cJSON *url = cJSON_GetObjectItemCaseSensitive(root, "url");
    const cJSON *sha = cJSON_GetObjectItemCaseSensitive(root, "sha256");
    if (!cJSON_IsObject(root) || !cJSON_IsString(url) || !url->valuestring ||
        strlen(url->valuestring) > OTA_URL_MAX ||
        !ota_url_allowed(url->valuestring, host_suffix, allow_any_host)) {
        res = OTA_CMD_BAD_URL;
    } else if (!cJSON_IsString(sha) || !ota_parse_sha256(sha->valuestring, out->sha256)) {
        res = OTA_CMD_BAD_SHA256;
    } else {
        strcpy(out->url, url->valuestring);
    }
    cJSON_Delete(root);
    return res;
}

const char *ota_cmd_result_str(ota_cmd_result_t r)
{
    switch (r) {
    case OTA_CMD_OK:         return "ok";
    case OTA_CMD_BAD_JSON:   return "json invalido";
    case OTA_CMD_BAD_URL:    return "url ausente, no https o host no permitido";
    case OTA_CMD_BAD_SHA256: return "sha256 ausente o mal formado";
    case OTA_CMD_BAD_VERSION: return "version ausente o demasiado larga";
    default:                 return "?";
    }
}

/* Siguiente numero de una version ("2.10.1-dev"): avanza *p; 0 si no hay mas */
static long next_num(const char **p)
{
    const char *s = *p;
    if (!*s || *s == '-' || *s == '+') return 0;
    long v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    if (*s == '.') s++;
    else while (*s && *s != '-' && *s != '+') s++; /* basura: se ignora hasta el sufijo */
    *p = s;
    return v;
}

int ota_version_cmp(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    for (int i = 0; i < 4; i++) {
        long x = next_num(&a), y = next_num(&b);
        if (x != y) return x > y ? 1 : -1;
    }
    return 0;
}

ota_cmd_result_t ota_validate_target(const char *payload, size_t len, const char *host_suffix,
                                     bool allow_any_host, ota_target_t *out)
{
    memset(out, 0, sizeof(*out));
    ota_cmd_result_t r = ota_validate_cmd(payload, len, host_suffix, allow_any_host, &out->job);
    if (r != OTA_CMD_OK) return r;
    cJSON *root = cJSON_ParseWithLength(payload, len);
    if (!root) return OTA_CMD_BAD_JSON;
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "version");
    if (!cJSON_IsString(v) || !v->valuestring || !v->valuestring[0] || strlen(v->valuestring) > OTA_VERSION_MAX) {
        r = OTA_CMD_BAD_VERSION;
    } else {
        strcpy(out->version, v->valuestring);
    }
    cJSON_Delete(root);
    return r;
}

bool ota_target_wanted(const char *running, const ota_target_t *t, const uint8_t *bad_sha)
{
    if (!t || !t->version[0]) return false;
    if (bad_sha && memcmp(bad_sha, t->job.sha256, 32) == 0) return false;
    return ota_version_cmp(t->version, running) > 0;
}
