/* Payload de incutwin/<id>/hand (spec hold-my-hand). C puro, probado en test_apps. */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAND_PAYLOAD_MAX 96

/* {"hand_hold":1,"incubator_id":"<id>","hold_s":<s>}
 * Devuelve la longitud escrita o -1 si no cabe. */
int hand_payload_build(char *out, size_t out_len, const char *incubator_id, unsigned hold_s);

#ifdef __cplusplus
}
#endif
