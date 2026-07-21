#include "i18n.h"

#include <Preferences.h>

#include "config.h"

int g_lang = DEFAULT_LANGUAGE;

static const char *STRINGS[STR_COUNT][2] = {
    /*                          ES                 EN            */
    /* STR_CONNECTION    */ {"Conexión", "Connection"},
    /* STR_WARMTH        */ {"Calor", "Warmth"},
    /* STR_LIGHT         */ {"Luz", "Light"},
    /* STR_HEART         */ {"Corazón", "Heart"},
    /* STR_ON            */ {"Encendida", "On"},
    /* STR_OFF           */ {"Apagado", "Off"},
    /* STR_HEATING       */ {"Calentando", "Heating"},
    /* STR_STABLE        */ {"Estable", "Stable"},
    /* STR_ALARM         */ {"¡Alarma!", "Alarm!"},
    /* STR_NO_DATA       */ {"Sin datos", "No data"},
    /* STR_CONNECTED     */ {"Conectada", "Connected"},
    /* STR_NO_CONNECTION */ {"Sin conexión", "No connection"},
    /* STR_SETTINGS      */ {"Ajustes", "Settings"},
    /* STR_LANGUAGE      */ {"Idioma", "Language"},
    /* STR_SKIN_TONE     */ {"Tono de piel", "Skin tone"},
    /* STR_SKIN_HINT     */
    {"Según el país de la IncuNest asignada",
     "Based on the assigned IncuNest's country"},
    /* STR_BACK          */ {"Volver", "Back"},
    /* STR_WIFI          */ {"WiFi", "WiFi"},
    /* STR_CLOUD         */ {"Nube", "Cloud"},
    /* STR_VERSION       */ {"Versión", "Version"},
    /* STR_SLEEPING      */ {"Durmiendo", "Sleeping"},
    /* STR_AWAKE         */ {"Despierto", "Awake"},
    /* STR_BEATING       */ {"Latiendo", "Beating"},
    /* STR_SOUND         */ {"Sonido", "Sound"},
    /* STR_ST_NO_WIFI    */ {"Sin conexión WiFi", "No WiFi connection"},
    /* STR_ST_CONNECTING */ {"Conectando a la nube...", "Connecting to the cloud..."},
    /* STR_ST_UNLINKED   */ {"Sin IncuNest vinculada", "No IncuNest linked"},
    /* STR_ST_OFF        */ {"IncuNest apagada", "IncuNest off"},
    /* STR_ST_NO_BABY    */ {"IncuNest sin bebé", "IncuNest empty"},
    /* STR_ST_BABY_SLEEP */ {"Bebé durmiendo", "Baby sleeping"},
    /* STR_ST_BABY_AWAKE */ {"Bebé despierto", "Baby awake"},
    /* STR_ST_PARENTS    */ {"Con sus papás", "With parents"},
    /* STR_HAND          */ {"Agarra mi mano", "Hold my hand"},
    /* STR_OB_LANG_TITLE */ {"Elige tu idioma", "Choose your language"},
    /* STR_OB_CONNECT_TITLE */ {"Conecta tu móvil", "Connect your phone"},
    /* STR_OB_CONNECT_STEPS */
    {"1. Escanea el código QR\n2. Sigue los pasos en tu móvil",
     "1. Scan the QR code\n2. Follow the steps on your phone"},
    /* STR_OB_NETWORK    */ {"Red", "Network"},
    /* STR_OB_CONNECTING */ {"Conectando a tu WiFi...", "Connecting to your WiFi..."},
    /* STR_OB_REGISTERING */ {"Registrando el panel...", "Registering the panel..."},
    /* STR_OB_WIFI_FAIL  */
    {"No se pudo conectar. Comprueba la contraseña.",
     "Could not connect. Check the password."},
    /* STR_OB_PAIR_TITLE */ {"Vincula la app", "Link the app"},
    /* STR_OB_PAIR_STEPS */
    {"Escanea con la app de IncuTwin\npara vincular tu panel",
     "Scan with the IncuTwin app\nto link your panel"},
    /* STR_OB_FINISH     */ {"Terminar", "Finish"},
};

const char *tr(str_id_t id) {
    if (id >= STR_COUNT) return "?";
    return STRINGS[id][g_lang ? 1 : 0];
}

void i18n_set_lang(int lang) {
    g_lang = lang ? 1 : 0;
    Preferences p;
    p.begin("incutwin", false);
    p.putUChar("lang", (uint8_t)g_lang);
    p.end();
}

void i18n_load(void) {
    Preferences p;
    p.begin("incutwin", true);
    g_lang = p.getUChar("lang", DEFAULT_LANGUAGE);
    p.end();
}
