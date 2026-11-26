/* Paginas deslizables de la pantalla principal (spec twin-display, "Páginas de la demo").
 * Bebe (lo de siempre), mapa con el pais, la distancia y la hora local (la IncuNest dibujada vuelve
 * al bebe; solo si se sabe el pais) y la racha con el saldo y el nivel (acompañar) o los logros
 * simulados (demo). Sellos, pasaporte y tienda: pantalla Corazones (ui_hearts.c). */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "lvgl.h"
#include "twin_types.h"

#define UI_PAGES_COUNT 3
/* Paneles (indices fijos; los que no tocan no estan en el orden de deslizar) */
enum { UI_PAGE_BABY = 0, UI_PAGE_MAP, UI_PAGE_STREAK };

/* Crea los paneles y los puntos sobre `scr`. Llamar despues de los objetos del bebe y antes de
 * la barra de estado y la barra superior, para que las tapen. */
void ui_pages_create(lv_obj_t *scr);

/* Con cada snapshot: fuera de demo vuelve a la pagina 0 y esconde los puntos. */
void ui_pages_apply(const twin_snapshot_t *s);

/* Deslizar: dir < 0 = siguiente (dedo a la izquierda), dir > 0 = anterior.
 * Devuelve true si ha cambiado de pagina. */
bool ui_pages_swipe(int dir);

/* Desliza hasta el panel indicado (UI_PAGE_*) si se puede ver; false si no (o si ya esta en el). */
bool ui_pages_goto(int page);

int ui_pages_current(void); /* el panel que se ve (UI_PAGE_*) */

/* Gesto "Coge mi mano" completado (suma a los logros de la demo). */
void ui_pages_note_hand(void);

void ui_pages_retranslate(void);

/* Saldo, nivel y fondo elegido (tras ui_rewards_refresh) */
void ui_pages_rewards_apply(void);

/* Demo: la semana simulada de la racha (bit 0 = hoy, que cuenta tras un gesto en esta demo) y si
 * hoy ya hay gesto. */
uint8_t ui_pages_demo_week(void);
bool ui_pages_demo_today(void);
