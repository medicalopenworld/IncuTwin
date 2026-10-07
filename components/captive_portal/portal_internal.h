#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

/* dns_responder.c: responde 192.168.4.1 a cualquier nombre (puerto 53 UDP). */
esp_err_t dns_responder_start(void);
void dns_responder_stop(void);

/* portal_pages.c: HTML del formulario y de la confirmacion en el idioma actual.
 * options: lista "<option>...</option>" ya construida. error: texto o NULL. */
void portal_page_form(char *out, size_t out_len, const char *options, const char *error);
void portal_page_done(char *out, size_t out_len);
const char *portal_text_fields_error(void);
