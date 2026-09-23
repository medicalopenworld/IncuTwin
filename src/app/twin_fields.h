#pragma once

/* Campos del estado del gemelo tal como los empuja ThingsBoard (shared
 * attributes del panel, rama "estado del gemelo" del Contrato v0.2):
 *
 *   online  bool     incubadora reportando
 *   thermo  string   off | heating | stable | alarm
 *   photo   bool     fototerapia
 *   hr      int      lpm redondeados a 5 (0 = sin pulso)
 *   baby    string   none | in | parents | out   (bool legado: true=in)
 *   home    bool     con baby=out: alta a casa
 *   skin    int      tono 0..5
 *   awake   bool
 *
 * Lo comparten tb_client y sim_server. */

#include <ArduinoJson.h>

/* Aplica un campo a g_state. Llamar CON state_lock() cogido.
 * Devuelve true si la clave es de estado del gemelo. */
bool twin_apply_field(const char *key, JsonVariantConst v);
