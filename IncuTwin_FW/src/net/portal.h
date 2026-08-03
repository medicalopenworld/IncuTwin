#pragma once

/* Portal cautivo de onboarding (SoftAP + DNS + formulario web).
 * El móvil se une a la red IncuTwin-XXXX (QR en pantalla) y rellena:
 * WiFi de casa, nombre, email y consentimiento RGPD.
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PORTAL_STOPPED = 0,
    PORTAL_WAITING,   /* AP activo, esperando formulario  */
    PORTAL_SUBMITTED, /* datos recibidos y guardados      */
} portal_state_t;

void portal_start(void);
void portal_stop(void);
portal_state_t portal_state(void);
const char *portal_ap_ssid(void);

#ifdef __cplusplus
}
#endif
