#pragma once

/* =========================================================================
 * IncuTwin — configuración
 *
 * Las credenciales WiFi y de usuario NO van aquí: se recogen en el
 * onboarding del primer arranque (portal cautivo) y se guardan en NVS.
 * ========================================================================= */

/* ---- ThingsBoard ----------------------------------------------------------
 * El panel se provisiona solo (Device Provisioning), recibe el estado del
 * gemelo como shared attributes (rama "estado del gemelo" del Contrato de
 * eventos v0.2), sube telemetría de uso y "coge mi mano", y hace OTA.
 *
 * Las claves de provisión del perfil "IncuTwin" NO están en el repo:
 * copia include/secrets.h.example a include/secrets.h (ignorado por git)
 * y pega ahí las del perfil en ThingsBoard (Device profiles -> IncuTwin ->
 * Device provisioning). Sin secrets.h compila, pero la provisión falla.
 */
#define TB_HOST "mon.medicalopenworld.org"  /* sin esquema */
#define TB_PORT 1883                        /* TODO: 8883 + TLS antes de fabricar */
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "include/secrets.h no encontrado: usando claves de provision ficticias"
#define TB_PROVISION_KEY "no-secrets-h-key"
#define TB_PROVISION_SECRET "no-secrets-h-secret"
#endif
#define TB_TELEMETRY_PERIOD_S 3600 /* telemetría de uso: 1 h (contadores acumulados) */
#define TB_MQTT_KEEPALIVE_S 60     /* keep-alive largo: menos tráfico con 10k paneles */
#define HAND_HOLD_MIN_INTERVAL_S 10 /* "coge mi mano": como mucho 1 envío cada 10 s */

/* ---- Onboarding / portal cautivo ------------------------------------------ */
#define AP_SSID_PREFIX "IncuTwin-"   /* + últimos 4 hex de la MAC */
#define AP_PASSWORD "incutwin"       /* mín. 8 caracteres (WPA2)   */
#define PAIR_URL_FMT "https://app.medicalopenworld.org/pair?sn=%s&code=%s"

/* ---- Comportamiento -------------------------------------------------------- */
#define DEFAULT_LANGUAGE 0        /* 0 = Español (por defecto), 1 = English */
#define DEFAULT_SKIN_TONE 1       /* hasta que llegue el del país asignado  */
#define SPLASH_MS 2500
#define DATA_STALE_S 180          /* incubadora sin conexión si no hay datos */
#define FACTORY_RESET_HOLD_MS 10000 /* mantener engranaje para reset         */

#define FW_TITLE "incutwin"       /* debe coincidir con el paquete OTA en TB */
#define FW_VERSION "1.1.0"
