/* IncuTwin — nombre local del bebe (spec nombre-bebe-panel).
 *
 * Pegamento de name_logic con la NVS (namespace "bname", blob "store"). El nombre es solo del
 * usuario de esta IncuTwin: nunca se publica ni se escribe en el log. Todas las funciones, salvo
 * baby_name_init, solo desde la tarea de la UI (con el lock de LVGL). */
#pragma once

#include <stdbool.h>

#include "twin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Carga el blob; uno de otra version o que no esta = vacio. Antes de ui_init. */
void baby_name_init(void);

/* Estancia actual: usa link.incubator_id, inc.stay_id e inc.baby. Guarda si algo cambio. */
void baby_name_observe(const twin_snapshot_t *s);

bool baby_name_should_ask(void);   /* estancia actual a la que aun no se ha preguntado */
const char *baby_name_local(void); /* nombre local de la estancia actual; "" si no hay */
const char *baby_name_key(void);   /* clave de la estancia actual ("" = ninguna); no se loguea */

/* Sobre la estancia `key` que tenia el aviso o el teclado al abrirse, no sobre la actual. */
void baby_name_dismiss(const char *key);          /* AHORA NO: no vuelve a preguntar */
bool baby_name_set(const char *key, const char *raw); /* guarda; false si queda vacio */

#ifdef __cplusplus
}
#endif
