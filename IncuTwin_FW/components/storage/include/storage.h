/* IncuTwin — acceso a NVS por namespace (spec identity-provisioning).
 *
 * Namespaces: "factory" y "mqtt" (fabricacion, sobreviven al factory reset),
 * "prov" (onboarding), "settings", "twin" (cache del gemelo), "usage", "bname" (nombre local).
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
#define STORAGE_NS_ACC      "acc"      /* modo Acompanar: historial (spec accompany) */
#define STORAGE_NS_BNAME    "bname"    /* nombre local del bebe (spec nombre-bebe-panel) */
#define STORAGE_NS_EVLOG    "evlog"    /* historial de avisos (event_log) */
#define STORAGE_NS_EVLOG_SIM "evlogsim" /* historial del build sim: no mezcla avisos falsos con los reales */
#define STORAGE_NS_STAYS    "stays"    /* bebes que han pasado por cada IncuNest (stay_log) */
#define STORAGE_NS_STAYS_SIM "stayssim" /* los del build sim */
#define STORAGE_NS_REWARDS  "rewards"  /* corazones, sellos, pasaporte, personalizar */
#define STORAGE_NS_REWARDS_SIM "rewardssim"
#define STORAGE_NS_INCUPRAY "incupray" /* IncuPray: avisos, apuesta y minutos de la ultima (spec incupray) */
#define STORAGE_NS_OTA      "ota"      /* OTA pendiente: se reintenta tras un corte (spec ota-update) */
#define STORAGE_NS_TEL      "tel"      /* telemetria: gen, boot_n, acked, fw, demo_n (sobrevive al reset) */
#define STORAGE_TEL_GEN     "gen"      /* tel/gen: +1 en cada restablecimiento de fabrica */

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

/* Blob de tamano exacto: false si no existe o su tamano no es `len`. */
bool storage_get_blob(const char *ns, const char *key, void *out, size_t len);
bool storage_set_blob(const char *ns, const char *key, const void *val, size_t len);
bool storage_erase_key(const char *ns, const char *key);
bool storage_erase_ns(const char *ns);

/* Borra prov, settings, twin, acc, bname, rewards e incupray (NO factory, mqtt, usage ni tel),
 * sube tel/gen (nuevo dueno para la telemetria; su cola en la particion `storage` no se toca) y
 * reinicia. */
void storage_factory_reset(void) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif
