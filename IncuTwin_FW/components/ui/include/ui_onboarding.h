/* IncuTwin — asistente de primer arranque (spec onboarding).
 *   1 idioma -> 2 QR del SoftAP (portal) -> 3 conectando a tu WiFi (25 s)
 *   -> 4 conectando con el servidor (30 s o MQTT_CONNECTED, o salto sin credenciales)
 *   -> 5 listo: serie en texto y QR, Terminar -> prov/done y reinicio.
 * Requiere board_init, net_wifi_init, settings_init y app_events_init previos. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void ui_onboarding_start(void);

/* Desde la pantalla principal (icono WiFi): solo los pasos de WiFi. Levanta el portal,
 * prueba la nueva red y reinicia si conecta; "Cancelar" tambien reinicia (vuelve a la red
 * guardada). No toca prov/done. */
void ui_onboarding_start_wifi_only(void);

#ifdef __cplusplus
}
#endif
