/* IncuTwin — emparejado por cmd/pair (spec pairing). Lo invoca commands.c. */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Se suscribe a TWIN_EVT_STATE_SUB_REJECTED: si el broker rechaza la suscripcion al estado
 * de la incubadora emparejada, el panel se desempareja solo. */
void pairing_init(void);

/* Procesa el payload de incutwin/<client_id>/cmd/pair (unicast; por flota se ignora). */
void pairing_handle(const char *payload, size_t len);

#ifdef __cplusplus
}
#endif
