#pragma once

/* Asistente de primer arranque:
 *   1. Idioma (ES por defecto)
 *   2. QR para unirse al SoftAP -> portal cautivo (WiFi, nombre, email, RGPD)
 *   3. Conexión WiFi + registro en ThingsBoard
 *   4. QR de vinculación con la app -> Terminar (reinicia)
 */

#ifdef __cplusplus
extern "C" {
#endif

void ui_onboarding_start(void);

#ifdef __cplusplus
}
#endif
