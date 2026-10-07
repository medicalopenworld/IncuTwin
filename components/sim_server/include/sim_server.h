/* IncuTwin — servidor web de simulacion (spec sim-server). Solo CONFIG_INCUTWIN_SIM.
 *   GET  /            pagina de control
 *   GET  /state       modelo actual (JSON)
 *   POST /state       {"scenario":"<id>", <campos sueltos>}
 *   POST /incubator   payload real de incubators/<id>/state (incubator_id "SIM") -> parser real
 *   POST /demo        {"run":bool,"interval_s":2..120}
 * El enlace se simula como correcto (sin MQTT ni OTA en esta build). */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void sim_server_start(void);

#ifdef __cplusplus
}
#endif
