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
} ota_cmd_result_t;

/* host_suffix: p. ej. ".medicalopenworld.org"; allow_any_host solo en builds dev. */
ota_cmd_result_t ota_validate_cmd(const char *payload, size_t len, const char *host_suffix,
                                  bool allow_any_host, ota_job_t *out);
const char *ota_cmd_result_str(ota_cmd_result_t r);

/* true si url es https:// y su host termina en host_suffix (o allow_any_host). */
bool ota_url_allowed(const char *url, const char *host_suffix, bool allow_any_host);

/* 64 hex -> 32 bytes. */
bool ota_parse_sha256(const char *hex, uint8_t out[32]);

#ifdef __cplusplus
}
#endif
