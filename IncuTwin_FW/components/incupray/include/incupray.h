/* IncuTwin — IncuPray (spec incupray). Encendida solo cuando la API del VPS publica el horario en
 * incupray/next (retenido): sin el, el panel no enseña nada ("dark launch"; lanzarla es solo
 * cosa del servidor, sin OTA).
 *
 * Pegamento de incupray_logic con el broker, la NVS y las recompensas: recibe incupray/next, live
 * y summary (desde commands), guarda lo que hay que recordar de cada IncuPray (avisos enseñados,
 * apuesta pagada, segundos con el dedo), cobra la apuesta y paga los corazones con rewards, y
 * publica la participacion en incutwin/<id>/incupray al unirse, cada 60 s y al salir. Publica
 * TWIN_EVT_INCUPRAY cuando cambia algo para la UI. La demo tiene una IncuPray simulada de 3 min
 * que no publica nada ni toca lo de verdad. Seguro entre tareas. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "incupray_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

void incupray_init(void); /* tras storage, app_events y rewards */
bool incupray_is_topic(const char *topic);
void incupray_ingest(const char *topic, const char *payload, size_t len); /* desde commands */

/* Hora para la IncuPray: epoch si el reloj esta en hora, si no 0 (sin IncuPray); en la demo, los
 * segundos desde el arranque */
uint32_t incupray_now(void);
/* La IncuPray que toca (la de verdad o la de la demo); false si no hay horario */
bool incupray_window(ip_window_t *out);
void incupray_live(ip_live_t *out);      /* lo ultimo de incupray/live (o lo simulado) */
bool incupray_demo_active(void);

/* El aviso que toca ahora (y lo marca como enseñado); IP_NOTICE_NONE si ninguno */
ip_notice_t incupray_take_notice(void);
/* Resumen de una IncuPray en la que no se entro, una sola vez; false si no hay */
bool incupray_take_missed_summary(ip_summary_t *out);
/* Resumen de la ultima IncuPray (para el gracias al cerrar); false si aun no llego */
bool incupray_summary(ip_summary_t *out);

/* Entrar (cobra la apuesta la primera vez: *paid = 3 o 0; devuelve true esa primera vez) y salir
 * (publica la participacion) */
bool incupray_join(uint16_t *paid);
void incupray_leave(void);
bool incupray_joined(void);
/* Dedo sobre un bebe: suma `ms` (solo con la ventana abierta) y paga 1 corazon por minuto */
void incupray_hold(uint32_t ms);
uint32_t incupray_held_s(void);
uint8_t incupray_hearts(void);

/* Demo: una IncuPray simulada de 3 min empezando ya; se corta al salir de la demo */
void incupray_demo_start(void);
void incupray_demo_stop(void);

#ifdef __cplusplus
}
#endif
