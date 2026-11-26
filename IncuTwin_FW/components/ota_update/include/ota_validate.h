/* Validacion pura del comando cmd/ota (spec ota-update). Probado en test_apps. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_URL_MAX 256

typedef struct {
    char url[OTA_URL_MAX + 1];
    uint8_t sha256[32];
} ota_job_t;

typedef enum {
    OTA_CMD_OK = 0,
    OTA_CMD_BAD_JSON,
    OTA_CMD_BAD_URL,     /* ausente, > 256 B, no https o host no permitido */
    OTA_CMD_BAD_SHA256,  /* ausente o no son 64 hex */
    OTA_CMD_BAD_VERSION, /* cmd/fw sin "version" o demasiado larga */
} ota_cmd_result_t;

#define OTA_VERSION_MAX 31

/* Firmware objetivo retenido (cmd/fw): la version que deberia llevar el panel y de donde bajarla */
typedef struct {
    ota_job_t job;
    char version[OTA_VERSION_MAX + 1];
} ota_target_t;

/* host_suffix: p. ej. ".medicalopenworld.org"; allow_any_host solo en builds dev. */
ota_cmd_result_t ota_validate_cmd(const char *payload, size_t len, const char *host_suffix,
                                  bool allow_any_host, ota_job_t *out);
const char *ota_cmd_result_str(ota_cmd_result_t r);

/* true si url es https:// y su host termina en host_suffix (o allow_any_host). */
bool ota_url_allowed(const char *url, const char *host_suffix, bool allow_any_host);

/* 64 hex -> 32 bytes. */
bool ota_parse_sha256(const char *hex, uint8_t out[32]);

/* "2.0.1" frente a "2.0.0": >0, 0 o <0, numero a numero; lo que va tras '-' o '+' no cuenta. */
int ota_version_cmp(const char *a, const char *b);

/* {"version":"2.1.0","url":"https://...","sha256":"<64 hex>"} */
ota_cmd_result_t ota_validate_target(const char *payload, size_t len, const char *host_suffix,
                                     bool allow_any_host, ota_target_t *out);

/* true si el panel (con la version `running`) debe ir al objetivo: solo si va por detras (nunca baja)
 * y el binario no es uno que ya fallo (`bad_sha`, NULL = ninguno). */
bool ota_target_wanted(const char *running, const ota_target_t *t, const uint8_t *bad_sha);

#ifdef __cplusplus
}
#endif
