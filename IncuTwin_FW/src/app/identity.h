#pragma once

/* Identidad del dispositivo y datos de provisión persistentes.
 *
 * - NVS "factory" (escrita en fabricación por tools/factory_provision.py):
 *     sn, hwrev, batch
 *   Si no existe, el SN se deriva de la MAC eFuse: ITW-XXXXXX.
 *
 * - NVS "prov" (escrita por el onboarding):
 *     done, ssid, pass, name, email, gdpr, tb_token
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void identity_init(void);

const char *identity_sn(void);     /* p.ej. "ITW-2607-0042"      */
const char *identity_hwrev(void);  /* "" si no hay NVS de fábrica */

bool prov_is_done(void);
void prov_mark_done(void);
void prov_factory_reset(void); /* borra "prov" y reinicia */

/* credenciales / datos de usuario */
bool prov_get_wifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len);
void prov_set_wifi(const char *ssid, const char *pass);
void prov_set_user(const char *name, const char *email, bool gdpr);
bool prov_get_name(char *out, size_t len);

/* token de acceso de ThingsBoard */
bool prov_get_tb_token(char *out, size_t len);
void prov_set_tb_token(const char *token);

/* código de emparejamiento con la app (6 dígitos, generado una vez) */
const char *prov_pair_code(void);

#ifdef __cplusplus
}
#endif
