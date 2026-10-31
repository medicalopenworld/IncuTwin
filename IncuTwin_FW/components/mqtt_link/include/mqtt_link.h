/* IncuTwin — sesion MQTT con el broker (spec broker-link, design.md D6).
 *
 * esp-mqtt sobre TLS (bundle de CAs, SNI por hostname), client id = usuario =
 * mqtt/user de NVS, keepalive 60 s, sesion limpia, LWT retenido en
 * incutwin/<id>/status. Reconexion dirigida por este modulo con backoff
 * exponencial 1 -> 60 s +-20 %. Arranca con la WiFi (TWIN_EVT_WIFI) y publica
 * TWIN_EVT_MQTT (conectado/desconectado) y TWIN_EVT_MQTT_MSG (cada mensaje
 * <= 1024 B). Al conectar se suscribe a incutwin/<id>/cmd/#, incutwin/all/cmd/#
 * y, si hay emparejado, a incubators/<incubator_id>/state (todo QoS 1).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ESP_ERR_INVALID_STATE si el panel no tiene credenciales (sin serializar). */
esp_err_t mqtt_link_start(void);

bool mqtt_link_connected(void);
const char *mqtt_link_client_id(void);

/* Cambia la suscripcion al estado de la incubadora (NULL o "" = ninguna). */
void mqtt_link_set_incubator(const char *incubator_id);

/* Publica (QoS 0/1, retain). Devuelve false si no hay sesion. */
bool mqtt_link_publish(const char *topic, const char *payload, size_t len, int qos, bool retain);

#ifdef __cplusplus
}
#endif
