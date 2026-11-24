#include "baby_name.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "name_logic.h"
#include "storage.h"

static const char *TAG = "bname";
#define KEY_STORE "store"

static bn_store_t s_st;
static char s_key[BN_KEY_LEN];

static void save(void)
{
    if (!storage_set_blob(STORAGE_NS_BNAME, KEY_STORE, &s_st, sizeof(s_st))) {
        ESP_LOGW(TAG, "no se pudo guardar");
    }
}

void baby_name_init(void)
{
    if (!storage_get_blob(STORAGE_NS_BNAME, KEY_STORE, &s_st, sizeof(s_st)) ||
        s_st.version != BN_STORE_VERSION) {
        bn_store_reset(&s_st);
    }
    s_key[0] = '\0';
}

void baby_name_observe(const twin_snapshot_t *s)
{
    const char *inc = bn_observed_inc(s); /* sin estado recibido no cuenta (cambio de emparejamiento) */
    char prev[BN_KEY_LEN];
    snprintf(prev, sizeof(prev), "%s", s_key);
    if (bn_observe(&s_st, inc, s->inc.stay_id, s->inc.baby, s_key)) save();
    /* al cambiar de estancia; nunca el nombre ni la clave */
    if (s_key[0] && strcmp(prev, s_key) != 0) {
        ESP_LOGI(TAG, "nombre local: %s", baby_name_local()[0] ? "si" : "no");
    }
}

bool baby_name_should_ask(void) { return bn_should_ask(&s_st, s_key); }

const char *baby_name_local(void) { return bn_name(&s_st, s_key); }

const char *baby_name_key(void) { return s_key; }

void baby_name_dismiss(const char *key)
{
    if (bn_mark_asked(&s_st, key)) save();
}

bool baby_name_set(const char *key, const char *raw)
{
    if (!bn_set_name(&s_st, key, raw)) return false;
    save();
    ESP_LOGI(TAG, "nombre local: si");
    return true;
}
