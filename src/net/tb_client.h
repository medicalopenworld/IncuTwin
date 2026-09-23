#pragma once

/* Cliente ThingsBoard (MQTT):
 *   - Device Provisioning con provision key/secret (primer arranque):
 *     obtiene un access token propio y lo guarda en NVS.
 *   - Estado del gemelo: shared attributes que empuja la rama "estado del
 *     gemelo" de la IncuNest emparejada (online, thermo, photo, hr, baby,
 *     home, name) -> g_state.
 *   - "Coge mi mano": telemetría {"hand_hold":1} al pulsar el botón.
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

/* "Coge mi mano" pulsado. Seguro desde el hilo de LVGL: solo marca la
 * petición; la tarea MQTT la publica (máx. 1 cada HAND_HOLD_MIN_INTERVAL_S). */
void tb_client_hand_hold(void);

#ifdef __cplusplus
}
#endif
