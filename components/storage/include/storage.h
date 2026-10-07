/* IncuTwin — acceso a NVS por namespace (spec identity-provisioning).
 *
 * Namespaces: "factory" y "mqtt" (fabricacion, sobreviven al factory reset),
 * "prov" (onboarding), "settings", "twin" (cache del gemelo), "usage".
 * Todas las funciones abren y cierran el namespace: pensadas para accesos
 * poco frecuentes, no para bucles calientes.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_NS_FACTORY  "factory"
#define STORAGE_NS_MQTT     "mqtt"
#define STORAGE_NS_PROV     "prov"
#define STORAGE_NS_SETTINGS "settings"
#define STORAGE_NS_TWIN     "twin"
#define STORAGE_NS_USAGE    "usage"

esp_err_t storage_init(void); /* nvs_flash_init con recuperacion si cambia la version */

/* Devuelven false si la clave no existe o no cabe. */
bool storage_get_str(const char *ns, const char *key, char *out, size_t out_len);
bool storage_get_u8(const char *ns, const char *key, uint8_t *out);
bool storage_get_u32(const char *ns, const char *key, uint32_t *out);
bool storage_get_i32(const char *ns, const char *key, int32_t *out);

bool storage_set_str(const char *ns, const char *key, const char *val);
bool storage_set_u8(const char *ns, const char *key, uint8_t val);
bool storage_set_u32(const char *ns, const char *key, uint32_t val);
bool storage_set_i32(const char *ns, const char *key, int32_t val);

bool storage_erase_key(const char *ns, const char *key);
bool storage_erase_ns(const char *ns);

/* Borra prov, settings y twin (NO factory, mqtt ni usage) y reinicia. */
void storage_factory_reset(void) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif
