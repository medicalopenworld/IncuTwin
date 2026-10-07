/* IncuTwin — estacion WiFi (spec wifi-connectivity).
 *
 * - net_wifi_init(): netif + driver en modo STA, sin conectar. Lo necesita
 *   tambien el portal cautivo (que conmuta a AP+STA por su cuenta).
 * - net_wifi_connect(): conecta y reintenta indefinidamente con espera 1, 2, 4,
 *   8, 16, 30 s (tope 30 s), reiniciada al conectar. Nunca reinicia el chip.
 * - Publica TWIN_EVT_WIFI (connected, rssi, ip) al obtener/perder IP y, mientras
 *   esta conectado, cada 1 s con el RSSI muestreado.
 * - Al obtener IP por primera vez arranca SNTP (CONFIG_INCUTWIN_SNTP_SERVER).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t net_wifi_init(void);
esp_err_t net_wifi_connect(const char *ssid, const char *pass);
void net_wifi_disconnect(void); /* para la reconexion y desconecta */

bool net_wifi_is_connected(void);
int8_t net_wifi_rssi(void);
bool net_wifi_time_valid(void); /* hora SNTP con ano >= 2025 */

#ifdef __cplusplus
}
#endif
