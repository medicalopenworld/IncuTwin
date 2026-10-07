/* IncuTwin — identidad del panel (spec identity-provisioning).
 *
 *   NVS "factory": sn, hwrev, batch        (tools/factory_provision.py)
 *   NVS "mqtt"   : user, pass, incubator_id (user/pass de fabrica; incubator_id por cmd/pair)
 *   NVS "prov"   : done, ssid, pass         (onboarding)
 *
 * Sin NVS de fabrica el serie se deriva de la MAC WiFi: ITW-XXXXXX.
 * La contrasena MQTT solo se entrega por identity_mqtt_pass() al cliente MQTT;
 * nunca se imprime.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "twin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void identity_init(void);

const char *identity_sn(void);    /* p. ej. "ITW-2640-0007" o "ITW-DEEF3C" */
const char *identity_hwrev(void); /* "" si no hay NVS de fabrica */
bool identity_has_factory_nvs(void);

/* Credenciales del broker. client id == user. */
bool identity_has_creds(void);
const char *identity_mqtt_user(void);               /* "" si no hay */
bool identity_mqtt_pass(char *out, size_t out_len); /* false si no hay */

/* Emparejado persistente (NVS mqtt/incubator_id). */
bool identity_incubator_id(char out[TWIN_INCUBATOR_ID_LEN]); /* false si no hay */
bool identity_set_incubator_id(const char *id);              /* NULL o "" = borrar */

/* Onboarding (NVS prov). */
bool identity_prov_done(void);
void identity_prov_mark_done(void);
bool identity_wifi_creds(char *ssid, size_t ssid_len, char *pass, size_t pass_len);
bool identity_set_wifi_creds(const char *ssid, const char *pass);

#ifdef __cplusplus
}
#endif
