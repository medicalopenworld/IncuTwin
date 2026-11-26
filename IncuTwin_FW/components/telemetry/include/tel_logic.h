/* IncuTwin — telemetria de uso: codificacion de eventos y lotes (spec telemetry-events).
 * Modulo puro (sin flash ni red): se prueba en test_apps. No depende de nada de la app.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TEL_KIND_MAX  12
#define TEL_D_MAX     160
#define TEL_BATCH_MAX 4096
#define TEL_BATCH_N   20
#define TEL_SCREEN_DEDUP_MS 5000

typedef struct {
    uint32_t seq;
    int64_t  t;                 /* epoch; 0 sin hora */
    uint32_t up;                /* s desde el arranque */
    char     kind[TEL_KIND_MAX];
    char     d[TEL_D_MAX];      /* objeto JSON ya armado, "{}" si nada */
} tel_event_t;

/* {"s":..,"t":..,"up":..,"k":"..","d":{..}}; longitud escrita, o -1 si no cabe */
int tel_event_encode(char *out, size_t len, const tel_event_t *e);

/* Mete en out tantos eventos de ev[0..n) como quepan (<= TEL_BATCH_N, <= TEL_BATCH_MAX bytes y len);
 * *used = cuantos. Devuelve la longitud, o -1 si ni el primero cabe. */
int tel_batch_build(char *out, size_t len, uint32_t gen, uint32_t boot,
                    const tel_event_t *ev, int n, int *used);

/* Filtro de pantallas: true si hay que registrar `page` (no la misma en < 5 s).
 * El que llama inicia *last_page a -1. */
bool tel_screen_should_log(int page, uint32_t now_ms, int *last_page, uint32_t *last_ms);

#ifdef __cplusplus
}
#endif
