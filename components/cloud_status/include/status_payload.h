/* Payload de incutwin/<id>/status (spec device-status). C puro, probado en test_apps. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STATUS_PAYLOAD_MAX 160

/* {"online":true,"fw":"<fw>","rssi":<rssi>,"incubator_id":"<id|>","ts":<ts>}
 * Devuelve la longitud escrita o -1 si no cabe. */
int status_payload_build(char *out, size_t out_len, const char *fw, int rssi,
                         const char *incubator_id, int64_t ts);

#ifdef __cplusplus
}
#endif
