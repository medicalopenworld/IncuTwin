/* Bebe sin WiFi (tira de Images/Baby_no_wifi.png, tools/gen_nowifi.py), nino y nina:
 *   - bucle sin WiFi (Inicio): chupete -> saca la antena -> "¡oh!" -> busca senal (el WiFi
 *     aparece por partes) -> aspa y rayo que parpadean; texto "Sin WiFi, buscando señal..."
 *   - busqueda (asistente del QR y Cambiar WiFi): saca la antena una vez y los arcos aparecen
 *     en bucle, sin aspa; texto "Conectando a tu WiFi..."
 *   - celebracion al conectar (una vez, ~3,6 s): se va el aspa, los arcos se encienden en azul,
 *     marca verde, dos saltos con corazones, estrellas y confeti desde la antena, "¡WiFi
 *     conectado!" y fundido
 * Respira 3 px como el bebe de Inicio. Una instancia por pantalla; se libera sola al borrar su
 * `parent` (las particulas van en el para poder salir del lienzo). Todo en la tarea de LVGL. */
#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef struct ui_nowifi ui_nowifi_t;

/* lienzo con la esquina en (x, y) de `parent`; el texto, centrado en la pantalla en text_y */
ui_nowifi_t *ui_nowifi_create(lv_obj_t *parent, int32_t x, int32_t y, int32_t text_y);
void ui_nowifi_show(ui_nowifi_t *nw, bool girl);   /* bucle sin WiFi (si no estaba ya) */
void ui_nowifi_search(ui_nowifi_t *nw, bool girl); /* busqueda del asistente */
void ui_nowifi_hide(ui_nowifi_t *nw);              /* lo para y lo oculta sin celebrar */
/* Celebra, se oculta y llama a done (al final, sin tocar ya la instancia: done puede borrar
 * la pantalla) */
void ui_nowifi_celebrate(ui_nowifi_t *nw, bool girl, void (*done)(void));
void ui_nowifi_skip(ui_nowifi_t *nw);              /* acaba ya la celebracion (toque) */
bool ui_nowifi_active(const ui_nowifi_t *nw);      /* bucle, busqueda o celebracion */
bool ui_nowifi_celebrating(const ui_nowifi_t *nw);
void ui_nowifi_retranslate(ui_nowifi_t *nw);
