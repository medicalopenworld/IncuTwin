#pragma once

/* Cliente ThingsBoard (MQTT):
 *   - Device Provisioning con provision key/secret (primer arranque):
 *     obtiene un access token propio y lo guarda en NVS.
 *   - Telemetría de uso periódica (horas encendido, conectado, Agarra mi
 *     mano) y estado.
 *   - OTA gestionada desde ThingsBoard (fw_title/fw_version + chunks).
 *
 * Corre en su propia tarea (core 0).
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TB_IDLE = 0,
    TB_PROVISIONING,
    TB_PROVISION_FAILED,
    TB_CONNECTED,
    TB_DISCONNECTED,
    TB_OTA_DOWNLOADING,
} tb_status_t;

void tb_client_start(void);
tb_status_t tb_status(void);
bool tb_has_token(void);

#ifdef __cplusplus
}
#endif
