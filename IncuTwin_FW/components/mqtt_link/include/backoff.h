/* Backoff exponencial con jitter (spec broker-link). C puro, probado en test_apps.
 *   1, 2, 4, 8, 16, 32, 60, 60... s (tope max_s), cada uno +-20 % de jitter.
 *   backoff_reset() tras una conexion correcta. */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t min_s;
    uint32_t max_s;
    uint32_t next_s; /* valor nominal del proximo intento */
} backoff_t;

void backoff_init(backoff_t *b, uint32_t min_s, uint32_t max_s);
void backoff_reset(backoff_t *b);

/* Devuelve el retardo nominal actual (s) y avanza al siguiente. */
uint32_t backoff_next_nominal(backoff_t *b);

/* Aplica +-20 % de jitter a un retardo nominal con la semilla dada (0..0xFFFFFFFF). */
uint32_t backoff_apply_jitter_ms(uint32_t nominal_s, uint32_t rnd);

#ifdef __cplusplus
}
#endif
