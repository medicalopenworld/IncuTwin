#include "ui_i18n.h"

#include "settings.h"

static const char *const STRINGS[STR_COUNT][2] = {
    /*                            ES                                  EN                              */
    /* STR_ST_HOME        */ { "¡Ya está en casa!",                 "Home at last!" },
    /* STR_ST_PARENTS     */ { "Con sus papás",                     "With parents" },
    /* STR_ST_NO_WIFI     */ { "Sin conexión WiFi",                 "No WiFi connection" },
    /* STR_ST_NO_CREDS    */ { "Panel sin serializar",              "Panel not provisioned" },
    /* STR_ST_CONNECTING  */ { "Conectando con el servidor...",     "Connecting to the server..." },
    /* STR_ST_BROKER_LOST */ { "Sin conexión con el servidor",      "No connection to the server" },
    /* STR_ST_UNPAIRED    */ { "Sin IncuNest vinculada",            "No IncuNest linked" },
    /* STR_ST_WAITING     */ { "Esperando a la IncuNest...",        "Waiting for the IncuNest..." },
    /* STR_ST_INC_OFF     */ { "IncuNest apagada",                  "IncuNest off" },
    /* STR_ST_NO_BABY     */ { "IncuNest sin bebé",                 "IncuNest empty" },
    /* STR_ST_ALARM       */ { "¡Alarma!",                          "Alarm!" },
    /* STR_ST_AWAKE       */ { "Bebé despierto",                    "Baby awake" },
    /* STR_ST_SLEEP       */ { "Bebé durmiendo",                    "Baby sleeping" },
    /* STR_DAY            */ { "día",                               "day" },
    /* STR_DAYS           */ { "días",                              "days" },
    /* STR_HAND           */ { "Agarra mi mano",                    "Hold my hand" },
    /* STR_SETTINGS       */ { "Ajustes",                           "Settings" },
    /* STR_LANGUAGE       */ { "Idioma",                            "Language" },
    /* STR_SKIN_TONE      */ { "Tono de piel",                      "Skin tone" },
    /* STR_SKIN_HINT      */ { "Según el país de la IncuNest asignada",
                               "Based on the assigned IncuNest's country" },
    /* STR_SOUND          */ { "Sonido",                            "Sound" },
    /* STR_VOL_OFF        */ { "Apagado",                           "Off" },
    /* STR_VOL_LOW        */ { "Bajo",                              "Low" },
    /* STR_VOL_MID        */ { "Medio",                             "Mid" },
    /* STR_VOL_HIGH       */ { "Alto",                              "High" },
    /* STR_BRIGHTNESS     */ { "Brillo",                            "Brightness" },
    /* STR_BACK           */ { "Volver",                            "Back" },
    /* STR_WIFI           */ { "WiFi",                              "WiFi" },
    /* STR_SERVER         */ { "Servidor",                          "Server" },
    /* STR_CONNECTED      */ { "Conectado",                         "Connected" },
    /* STR_NO_CONNECTION  */ { "Sin conexión",                      "No connection" },
    /* STR_INCUNEST       */ { "IncuNest",                          "IncuNest" },
    /* STR_RESET_Q        */ { "¿Restablecer de fábrica?",          "Factory reset?" },
    /* STR_RESET          */ { "Reset",                             "Reset" },
    /* STR_OB_CONNECT_TITLE */ { "Conecta tu móvil",                "Connect your phone" },
    /* STR_OB_CONNECT_STEPS */ { "1. Escanea el código QR\n2. Sigue los pasos en tu móvil",
                                 "1. Scan the QR code\n2. Follow the steps on your phone" },
    /* STR_OB_NETWORK     */ { "Red",                               "Network" },
    /* STR_OB_CONNECTING_WIFI   */ { "Conectando a tu WiFi...",     "Connecting to your WiFi..." },
    /* STR_OB_CONNECTING_SERVER */ { "Conectando con el servidor...", "Connecting to the server..." },
    /* STR_OB_WIFI_FAIL   */ { "No se pudo conectar. Comprueba la contraseña.",
                               "Could not connect. Check the password." },
    /* STR_OB_DONE_TITLE  */ { "¡Listo!",                           "All set!" },
    /* STR_OB_DONE_TEXT   */ { "Medical Open World vinculará tu IncuTwin con una IncuNest. "
                               "Verás al bebé en cuanto esté lista.",
                               "Medical Open World will link your IncuTwin to an IncuNest. "
                               "You'll see the baby as soon as it's ready." },
    /* STR_OB_FINISH      */ { "Terminar",                          "Finish" },
    /* STR_OB_CANCEL      */ { "Cancelar",                          "Cancel" },
    /* STR_WIFI_CHANGE_Q  */ { "¿Cambiar la red WiFi?",             "Change the WiFi network?" },
    /* STR_WIFI_CHANGE    */ { "Cambiar",                           "Change" },
};

const char *tr(ui_str_t id)
{
    if (id >= STR_COUNT) return "?";
    return STRINGS[id][settings_lang() == SETTINGS_LANG_EN ? 1 : 0];
}
