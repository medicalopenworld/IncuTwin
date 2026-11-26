/* Eventos de telemetria derivados de dos estados de rewards (modulo puro). Vive en rewards y no en
 * telemetry para que telemetry no dependa de rewards: el que llama pasa el sink. */
#pragma once

#include "rewards_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rw_tel_sink_t)(const char *kind, const char *d, void *ctx);

/* Eventos entre dos estados (news puede ser NULL; hoy no hace falta, todo sale de los estados).
 * Llama a sink(kind, d, ctx) en este orden: heart, seal (uno por bit), level, streak, passport,
 * spend. `d` es un objeto JSON de < 96 bytes. */
void tel_rewards_diff(const rw_state_t *before, const rw_state_t *after, const rw_news_t *news,
                      const char *why, rw_tel_sink_t sink, void *ctx);

#ifdef __cplusplus
}
#endif
