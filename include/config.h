#pragma once

/* =========================================================================
 * IncuTwin — configuración
 *
 * Las credenciales WiFi y de usuario NO van aquí: se recogen en el
 * onboarding del primer arranque (portal cautivo) y se guardan en NVS.
 * ========================================================================= */

/* ---- ThingsBoard ----------------------------------------------------------
 * El panel se provisiona solo (Device Provisioning) y sube telemetría de
 * uso. La OTA también se gestiona desde ThingsBoard.
 * ATENCIÓN: claves del ENTORNO DE PRUEBA. No publicar este archivo.
 */
#define TB_HOST "mon.medicalopenworld.org"  /* sin esquema */
#define TB_PORT 1883
#define TB_PROVISION_KEY "9l35qc4g5ejvwl9cs0sc"
#define TB_PROVISION_SECRET "uqm7j8f5jmpu3lztruku"
#define TB_TELEMETRY_PERIOD_S 3600 /* telemetría de uso: 1 h (contadores acumulados) */
#define TB_MQTT_KEEPALIVE_S 60     /* keep-alive largo: menos tráfico con 10k paneles */

/* ---- Firebase Realtime Database -------------------------------------------
 * Estado del gemelo (escrito por el puente ThingsBoard->Firebase).
 * Ver docs/FIREBASE_SETUP.md y docs/FIREBASE_SCHEMA.md.
 */
#define FIREBASE_HOST "your-project-default-rtdb.europe-west1.firebasedatabase.app"
#define FIREBASE_AUTH ""
#define FIREBASE_STATE_PATH_FMT "/incutwin/%s/state" /* %s = serial number */

/* ---- Onboarding / portal cautivo ------------------------------------------ */
#define AP_SSID_PREFIX "IncuTwin-"   /* + últimos 4 hex de la MAC */
#define AP_PASSWORD "incutwin"       /* mín. 8 caracteres (WPA2)   */
#define PAIR_URL_FMT "https://app.medicalopenworld.org/pair?sn=%s&code=%s"

/* ---- Comportamiento -------------------------------------------------------- */
#define DEFAULT_LANGUAGE 0        /* 0 = Español (por defecto), 1 = English */
#define DEFAULT_SKIN_TONE 1       /* hasta que llegue el del país asignado  */
#define SPLASH_MS 2500
#define DATA_STALE_S 90           /* incubadora sin conexión si no hay datos */
#define FACTORY_RESET_HOLD_MS 10000 /* mantener engranaje para reset         */

#define FW_TITLE "incutwin"       /* debe coincidir con el paquete OTA en TB */
#define FW_VERSION "1.1.0"
