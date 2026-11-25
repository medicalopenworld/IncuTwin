/* IncuTwin — modo Acompañar (spec accompany, openspec hmi-accompany).
 *
 * La IncuTwin se une sola a una IncuNest con bebe (prefiere la que mas ha acompañado), cuenta el
 * tiempo acompañado tras el gesto "Coge mi mano" y avisa en el panel. Pegamento de acc_logic:
 * NVS (mqtt/acc_mode, acc/state), ultimo estado de cada IncuNest en PSRAM, hora de España,
 * silencio nocturno y eventos. Seguro desde cualquier tarea. */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Antes de twin_model_init y mqtt_link_start. */
void accompany_init(void);
/* Tras twin_model_init: aplica la IncuNest guardada al gemelo y se suscribe a los eventos. */
void accompany_start(void);

bool accompany_enabled(void);
/* IncuNest que se acompaña ("" = ninguna). */
void accompany_current(char out[17]);
/* Lo llama pairing: true con {"mode":"accompany"}, false con un id o un desemparejado.
 * Al encender empieza buscando (conserva el historial). */
void accompany_set_enabled(bool on);

/* Estado de cualquier incubators/<id>/state (solo con el modo encendido). */
void accompany_ingest(const char *json, size_t len);

/* Cada segundo (tick de app_main). */
void accompany_tick_1s(void);

#ifdef __cplusplus
}
#endif
