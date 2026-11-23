/* Paginas deslizables de la pantalla principal (spec twin-display, "Páginas de la demo").
 * Solo con la demo activa: pagina 0 = el bebe (lo de siempre), 1 = pais, hospital y hora local,
 * 2 = logros acumulados. Los datos de las paginas 1 y 2 son simulados (Camerun). */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "lvgl.h"
#include "twin_types.h"

#define UI_PAGES_COUNT 3

/* Crea los paneles y los puntos sobre `scr`. Llamar despues de los objetos del bebe y antes de
 * la barra de estado y la barra superior, para que las tapen. */
void ui_pages_create(lv_obj_t *scr);

/* Con cada snapshot: fuera de demo vuelve a la pagina 0 y esconde los puntos. */
void ui_pages_apply(const twin_snapshot_t *s);

/* Deslizar: dir < 0 = siguiente (dedo a la izquierda), dir > 0 = anterior.
 * Devuelve true si ha cambiado de pagina. */
bool ui_pages_swipe(int dir);

/* Desliza hasta la pagina indicada si se puede ver; false si no (o si ya esta en ella). */
bool ui_pages_goto(int page);

int ui_pages_current(void);

/* Gesto "Coge mi mano" completado (suma a los logros de la demo). */
void ui_pages_note_hand(void);

void ui_pages_retranslate(void);

/* En la pagina del mapa: la hora del pais para la barra de estado ("En Togo son las 17:43").
 * false si no se esta viendo el mapa o aun no hay hora. */
bool ui_pages_status_text(char *buf, size_t n);
