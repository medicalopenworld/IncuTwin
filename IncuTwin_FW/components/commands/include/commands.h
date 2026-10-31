/* IncuTwin — despacho de mensajes MQTT (spec device-commands).
 *   incubators/<id>/state           -> twin_model_ingest
 *   incutwin/<client_id>/cmd/<name> -> pair | ota | reboot | test_melody | brightness
 *   incutwin/all/cmd/<name>         -> igual salvo pair (ignorado); ota/reboot con retardo aleatorio
 * Los handlers del bus de eventos no bloquean: reinicios y OTA se difieren. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void commands_start(void);

#ifdef __cplusplus
}
#endif
