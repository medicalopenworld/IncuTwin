/* IncuTwin — portal cautivo de onboarding (spec captive-portal).
 *
 * SoftAP "IncuTwin-XXXX" / "incutwin" en 192.168.4.1, DNS comodin y formulario
 * web solo-WiFi (SSID escaneado + contrasena). Las sondas de Android/iOS/Windows
 * reciben 302 al formulario. Al enviar, guarda las credenciales (identity) y
 * pasa a PORTAL_SUBMITTED; sigue activo para reintentar si la WiFi falla.
 * Requiere net_wifi_init() previo (driver WiFi arrancado en modo STA).
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PORTAL_STOPPED = 0,
    PORTAL_WAITING,   /* AP activo, esperando formulario */
    PORTAL_SUBMITTED, /* credenciales recibidas y guardadas */
} portal_state_t;

esp_err_t portal_start(void);
void portal_stop(void);
portal_state_t portal_state(void);
void portal_rearm(void);           /* vuelve a WAITING tras un intento fallido */
const char *portal_ap_ssid(void);  /* "IncuTwin-XXXX" */
const char *portal_ap_password(void);

#ifdef __cplusplus
}
#endif
