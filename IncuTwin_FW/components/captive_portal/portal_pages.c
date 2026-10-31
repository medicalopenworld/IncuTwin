/* Paginas HTML del portal (estilo IncuTwin, sin dependencias externas). */
#include <stdio.h>

#include "portal_internal.h"
#include "settings.h"

static const char PAGE_HEAD[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>IncuTwin</title><style>"
    "body{font-family:sans-serif;background:#F4F3F0;color:#1E3E6E;max-width:420px;margin:0 auto;padding:16px}"
    "h1{color:#054E92;font-size:1.4em}label{display:block;margin-top:14px;font-weight:bold}"
    "input,select{width:100%;padding:12px;margin-top:4px;font-size:1em;border:2px solid #054E92;"
    "border-radius:10px;box-sizing:border-box}"
    "button{width:100%;margin-top:20px;padding:14px;font-size:1.1em;background:#054E92;color:#fff;"
    "border:0;border-radius:12px}.err{color:#E0524A;font-weight:bold}"
    "</style></head><body><h1>IncuTwin</h1>";

static bool es(void) { return settings_lang() == SETTINGS_LANG_ES; }

void portal_page_form(char *out, size_t out_len, const char *options, const char *error)
{
    snprintf(out, out_len,
             "%s<p>%s</p>%s%s%s"
             "<form method='POST' action='/save'>"
             "<label>%s</label><select name='ssid'>%s</select>"
             "<label>%s</label><input type='password' name='pass' maxlength='63'>"
             "<button type='submit'>%s</button></form></body></html>",
             PAGE_HEAD,
             es() ? "Configura tu panel IncuTwin." : "Set up your IncuTwin panel.",
             error ? "<p class='err'>" : "", error ? error : "", error ? "</p>" : "",
             es() ? "Red WiFi de casa" : "Home WiFi network", options,
             es() ? "Contraseña WiFi" : "WiFi password",
             es() ? "Conectar" : "Connect");
}

void portal_page_done(char *out, size_t out_len)
{
    snprintf(out, out_len, "%s<p style='font-size:1.2em'>%s</p></body></html>", PAGE_HEAD,
             es() ? "¡Datos recibidos! Mira la pantalla del panel para continuar."
                  : "Done! Look at the panel screen to continue.");
}

const char *portal_text_fields_error(void)
{
    return es() ? "Revisa los campos marcados." : "Please check the fields.";
}
