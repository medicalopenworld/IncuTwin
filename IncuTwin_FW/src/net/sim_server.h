#pragma once

/* Servidor web de simulación (solo build _sim, -D SIM_MODE).
 * Página de control en http://<ip>/ para fijar g_state a mano,
 * aplicar escenarios predefinidos o rotar una demo automática.
 * Sustituye a tb_client como única fuente de g_state. */

#ifdef __cplusplus
extern "C" {
#endif

void sim_server_start(void);

#ifdef __cplusplus
}
#endif
