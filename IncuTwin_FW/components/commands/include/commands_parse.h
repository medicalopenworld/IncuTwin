/* Parseo puro de topics y payloads de comandos (specs device-commands, pairing).
 * Sin IDF: probado en test_apps. */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "twin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CMD_NAME_MAX 24

typedef enum {
    CMD_TOPIC_NONE = 0,  /* no es un topic de comando */
    CMD_TOPIC_UNICAST,   /* incutwin/<client_id>/cmd/<name> */
    CMD_TOPIC_FLEET,     /* incutwin/all/cmd/<name> */
} cmd_topic_kind_t;

/* Extrae <name> (sin '/', 1..23 chars). NONE si no encaja o tiene niveles extra. */
cmd_topic_kind_t cmd_parse_topic(const char *topic, const char *client_id, char name[CMD_NAME_MAX]);

/* true si el topic es incubators/<id>/state; copia <id> si out != NULL. */
bool cmd_is_state_topic(const char *topic, char out_id[TWIN_INCUBATOR_ID_LEN]);

typedef enum {
    PAIR_INVALID = 0,
    PAIR_UNPAIR,  /* payload vacio */
    PAIR_SET,     /* {"incubator_id":"<id valido>"} */
} pair_cmd_t;

pair_cmd_t cmd_parse_pair(const char *payload, size_t len, char out_id[TWIN_INCUBATOR_ID_LEN]);

/* {"value": n} -> clamp(n, 10, 100). false si no hay entero. */
bool cmd_parse_brightness(const char *payload, size_t len, int *out);

/* {"melody":"boot|baby|parents|test"} -> nombre; "" si no viene o no es valido. */
void cmd_parse_melody(const char *payload, size_t len, char out[16]);

#ifdef __cplusplus
}
#endif
