/* IncuTwin — nombre local del bebe: logica pura (spec nombre-bebe-panel).
 *
 * El usuario de la IncuTwin pone un nombre al bebe que no lo trae (solo para el, nunca sale del
 * panel). Aqui se decide que estancia es (stay_id de ThingsBoard o, si no llega, un contador de
 * llegadas por incubadora), si hay que preguntar y que nombre hay. Sin ESP-IDF ni LVGL: lo usan
 * baby_name.c (NVS) y los tests on-target. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "twin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BN_RING 8                 /* estancias recordadas */
#define BN_INCS 4                 /* contadores de llegada por incubadora */
#define BN_NAME_MAX_CHARS 12
#define BN_NAME_BYTES 25          /* 12 letras de 2 bytes (Ñ, tildes) + 0 */
#define BN_KEY_LEN TWIN_STAY_LEN  /* stay_id, o "<incubator_id>#<gen>" */
#define BN_STORE_VERSION 1

typedef struct {
    char key[BN_KEY_LEN]; /* "" = libre */
    char name[BN_NAME_BYTES];
    bool asked;
    uint32_t used;
} bn_entry_t;

typedef struct {
    char inc[TWIN_INCUBATOR_ID_LEN]; /* "" = libre */
    uint16_t gen;
    bool was_in;                     /* ultimo estado visto: dentro o con sus papas */
    uint32_t used;
} bn_gen_t;

typedef struct {
    uint8_t version;
    uint32_t clock;
    bn_entry_t e[BN_RING];
    bn_gen_t g[BN_INCS];
} bn_store_t;

void bn_store_reset(bn_store_t *st);

/* Actualiza los contadores y deja en key_out la clave de la estancia ("" si el bebe no esta
 * dentro ni con sus papas, o sin incubadora). Devuelve true si el store cambio (hay que
 * guardarlo). */
bool bn_observe(bn_store_t *st, const char *incubator_id, const char *stay_id, twin_baby_t baby,
                char key_out[BN_KEY_LEN]);

const char *bn_name(const bn_store_t *st, const char *key); /* "" si no hay */
bool bn_should_ask(const bn_store_t *st, const char *key);  /* false con key "" */
bool bn_mark_asked(bn_store_t *st, const char *key);        /* true si cambio */
bool bn_set_name(bn_store_t *st, const char *key, const char *raw); /* limpia; vacio = no cambia */
bool bn_clean_name(const char *raw, char out[BN_NAME_BYTES]);      /* false si queda vacio */

/* Teclado: la tecla (mayuscula ASCII o Latin-1 de 2 bytes: Ñ, Á...) en mayuscula o minuscula. */
void bn_key_case(const char *label, bool upper, char out[5]);
/* Mayuscula al empezar el nombre y tras un espacio. */
bool bn_upper_next(const char *text);

#ifdef __cplusplus
}
#endif
