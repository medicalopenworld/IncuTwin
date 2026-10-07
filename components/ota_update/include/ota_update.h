/* IncuTwin — actualizacion OTA por HTTPS con sha256 y rollback (spec ota-update, D7).
 *
 * - ota_update_start(): si la app esta pendiente de verificar (primer arranque tras
 *   una OTA) arma el watchdog de CONFIG_INCUTWIN_OTA_VALIDATE_TIMEOUT_S y espera
 *   TWIN_EVT_MQTT conectado para marcarla valida; si vence, marca invalida y
 *   reinicia (el bootloader vuelve al slot anterior).
 * - ota_update_request(): llamada por commands con el payload de cmd/ota. Valida
 *   (https, host *.medicalopenworld.org salvo build dev, sha256), un trabajo a la
 *   vez; por flota espera 0..CONFIG_INCUTWIN_OTA_FLEET_DELAY_MAX_S y un unicast
 *   lo adelanta. La descarga va en una tarea propia: esp_http_client + esp_ota_write
 *   + sha256 acumulado; solo si el hash coincide se activa el slot y se reinicia.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void ota_update_start(void);
void ota_update_request(const char *payload, size_t len, bool fleet);

#ifdef __cplusplus
}
#endif
