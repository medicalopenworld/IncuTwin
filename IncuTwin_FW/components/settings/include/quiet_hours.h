/* IncuTwin — horario nocturno: función pura, sin dependencias de IDF. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ¿Toca silencio a esta hora local? Intervalo [from, to); from > to cruza la
 * medianoche. Sin hora (hour < 0), apagado o from == to: nunca. */
bool quiet_hours_active(int hour, bool on, uint8_t from, uint8_t to);

#ifdef __cplusplus
}
#endif
