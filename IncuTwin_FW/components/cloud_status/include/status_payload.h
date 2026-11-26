/* Payload de incutwin/<id>/status (spec device-status). C puro, probado en test_apps. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STATUS_PAYLOAD_MAX 240 /* con el nombre de la IncuTwin (hasta 48 B) */

/* {"online":true,"fw":"<fw>","rssi":<rssi>,"incubator_id":"<id|>","ts":<ts>}
 * acc = modo Acompañar: añade "acc":1 antes de ts (incubator_id = la IncuNest que acompaña).
 * Devuelve la longitud escrita o -1 si no cabe. */
int status_payload_build(char *out, size_t out_len, const char *fw, int rssi,
                         const char *incubator_id, bool acc, int64_t ts);
/* Igual, con "name" (nombre de la IncuTwin, spec device-status «Nombre en el estado») si no esta
 * vacio; las comillas y barras del nombre se quitan para que el JSON sea valido. */
int status_payload_build_named(char *out, size_t out_len, const char *fw, int rssi,
                               const char *incubator_id, bool acc, int64_t ts, const char *name);

#ifdef __cplusplus
}
#endif
