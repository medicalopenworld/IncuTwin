/* IncuTwin — publicacion de incutwin/<id>/status (spec device-status):
 * al conectar, cada CONFIG_INCUTWIN_STATUS_PERIOD_S y al cambiar el emparejado.
 * Retenido, QoS 1. Nada mas se publica. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void cloud_status_start(void);

#ifdef __cplusplus
}
#endif
