/* IncuTwin — actualizacion OTA por HTTPS con sha256 y rollback (spec ota-update, D7).
 *
 * - ota_update_start(): si la app esta pendiente de verificar (primer arranque tras
 *   una OTA) arma el watchdog de CONFIG_INCUTWIN_OTA_VALIDATE_TIMEOUT_S y espera
 *   TWIN_EVT_MQTT conectado para marcarla valida; si vence, marca invalida y
 *   reinicia (el bootloader vuelve al slot anterior).
 * - ota_update_request(): llamada por commands con el payload de cmd/ota. Valida
 *   (https, host *.medicalopenworld.org salvo build dev, sha256), un trabajo a la
 *   vez; por flota espera 0..CONFIG_INCUTWIN_OTA_FLEET_DELAY_MAX_S y un unicast
 *   lo adelanta. Al quedar listo NO descarga: publica TWIN_EVT_OTA_PENDING y espera a que
 *   la UI elija el momento (spec ota-update, «Ventana de la actualización») y llame a
 *   ota_update_go(). La descarga va en una tarea propia: esp_http_client + esp_ota_write +
 *   sha256 acumulado, con el progreso en ota_update_progress(); si el hash coincide se
 *   activa el slot y se reinicia en seguida.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_PROGRESS_IDLE    (-1) /* nada en marcha */
#define OTA_PROGRESS_FAILED  (-2) /* la ultima descarga fallo; el panel sigue como estaba */

void ota_update_start(void);
void ota_update_request(const char *payload, size_t len, bool fleet);
/* Firmware objetivo retenido (cmd/fw, spec ota-update «Firmware objetivo»): si el panel va por
 * detras, OTA (la del panel en seguida, la de la flota con retardo aleatorio). Payload vacio = se
 * borra ese objetivo. El del panel manda sobre el de la flota; nunca se baja de version y un binario
 * que ya fallo no se reintenta. */
void ota_update_target(const char *payload, size_t len, bool fleet);
/* Hay una OTA esperando a que la UI la lance */
bool ota_update_pending(void);
/* Lanza la descarga de la OTA pendiente (desde la UI, con la ventana ya en pantalla) */
void ota_update_go(void);
/* 0..100 mientras descarga (100 = instalada, reinicia en ~1,5 s); OTA_PROGRESS_IDLE u
 * OTA_PROGRESS_FAILED. Se puede leer desde cualquier tarea. */
int ota_update_progress(void);

#ifdef __cplusplus
}
#endif
