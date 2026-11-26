/* IncuTwin — aviso "Ha llegado un bebe" y teclado para ponerle nombre (spec nombre-bebe-panel).
 * Solo desde la tarea de LVGL. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

void ui_name_ask(void);  /* tarjeta modal: SI abre el teclado, AHORA NO (o cerrar) no vuelve a preguntar */
void ui_name_edit(void); /* teclado a pantalla completa con el nombre local actual */
/* El mismo teclado para un texto cualquiera (nombre de la IncuTwin): LISTO llama a submit con el
 * texto (puede ser vacio); cancelar no llama a nada. No hace caso de ui_name_close(). */
void ui_name_edit_text(const char *title, const char *initial, uint32_t max_chars, void (*submit)(const char *text));
bool ui_name_open(void); /* aviso o teclado visibles */
const char *ui_name_open_key(void); /* estancia del aviso o del teclado abiertos */
void ui_name_close(void); /* cierra sin contar como preguntado (el estado ha cambiado) */
void ui_name_on_done(void (*cb)(void)); /* se llama al cerrar el aviso o el teclado */
