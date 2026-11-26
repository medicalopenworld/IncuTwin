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
/* Modo Acompañar: suscripcion a incubators/+/state en vez de a una sola (spec accompany). */
void mqtt_link_set_accompany(bool on);

bool mqtt_link_publish(const char *topic, const char *payload, size_t len, int qos, bool retain);

/* Como mqtt_link_publish() pero devuelve el msg_id de esp-mqtt (> 0 con QoS 1) o -1 si no hay
 * sesion o falla el envio (0 con QoS 0 enviado). Con QoS 1, al llegar el PUBACK se publica TWIN_EVT_MQTT_PUBACK
 * (app_evt_puback_t) con ese msg_id. Con QoS 0 el id no es util (no hay PUBACK).
 *
 * Hilos: el PUBACK se detecta en la tarea de esp-mqtt y se reenvia por app_events; el handler
 * corre en la tarea del bucle de eventos, nunca en la de esp-mqtt. El msg_id se devuelve al
 * volver esp_mqtt_client_publish(): el PUBACK puede llegar al handler antes de que el llamador
 * guarde el id, asi que quien llama debe anotar el id sin depender de haberlo visto antes.
 *
 * Desconexion: la sesion es limpia, asi que el broker no recuerda nada. Si cae la conexion
 * antes del PUBACK, quien llama debe reenviar desde su cola (no hay evento de fallo). Ojo: el
 * cliente esp-mqtt conserva su outbox en la misma instancia y puede reenviar al reconectar un
 * mensaje no confirmado (con DUP) mientras no haya caducado (30 s por defecto): puede llegar un
 * PUBACK tardio o repetido de un msg_id anterior. Hay que tolerar PUBACK de ids desconocidos. */
int mqtt_link_publish_id(const char *topic, const char *payload, size_t len, int qos, bool retain);

#ifdef __cplusplus
}
#endif
