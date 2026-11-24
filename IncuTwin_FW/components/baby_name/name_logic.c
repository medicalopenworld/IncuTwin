#include "name_logic.h"

#include <stdio.h>
#include <string.h>

void bn_store_reset(bn_store_t *st)
{
    memset(st, 0, sizeof(*st));
    st->version = BN_STORE_VERSION;
}

static bn_entry_t *find(bn_store_t *st, const char *key)
{
    if (!key || !key[0]) return NULL;
    for (int i = 0; i < BN_RING; i++) {
        if (strcmp(st->e[i].key, key) == 0) return &st->e[i];
    }
    return NULL;
}

/* Entrada para `key`: la existente, una libre o la menos usada (se pierde la mas antigua). */
static bn_entry_t *slot(bn_store_t *st, const char *key)
{
    bn_entry_t *e = find(st, key);
    if (e) return e;
    e = &st->e[0];
    for (int i = 0; i < BN_RING; i++) {
        if (!st->e[i].key[0]) { e = &st->e[i]; break; }
        if (st->e[i].used < e->used) e = &st->e[i];
    }
    memset(e, 0, sizeof(*e));
    snprintf(e->key, sizeof(e->key), "%s", key);
    return e;
}

/* Contador de la incubadora: el existente, uno libre o el de la llegada mas antigua. `used` solo
 * se mueve al crearlo y en cada llegada: observar sin cambios no obliga a escribir la NVS. */
static bn_gen_t *gen_for(bn_store_t *st, const char *inc, bool *changed)
{
    for (int i = 0; i < BN_INCS; i++) {
        if (strcmp(st->g[i].inc, inc) == 0) return &st->g[i];
    }
    bn_gen_t *g = NULL;
    for (int i = 0; i < BN_INCS && !g; i++) {
        if (!st->g[i].inc[0]) g = &st->g[i];
    }
    if (!g) {
        g = &st->g[0];
        for (int i = 1; i < BN_INCS; i++) {
            if (st->g[i].used < g->used) g = &st->g[i];
        }
    }
    memset(g, 0, sizeof(*g));
    snprintf(g->inc, sizeof(g->inc), "%s", inc);
    g->used = ++st->clock;
    *changed = true;
    return g;
}

bool bn_observe(bn_store_t *st, const char *incubator_id, const char *stay_id, twin_baby_t baby,
                char key_out[BN_KEY_LEN])
{
    key_out[0] = '\0';
    if (!incubator_id || !incubator_id[0]) return false;
    bool changed = false;
    bool in = baby == TWIN_BABY_IN || baby == TWIN_BABY_PARENTS;
    bn_gen_t *g = gen_for(st, incubator_id, &changed);
    if (in && !g->was_in) { g->gen++; g->used = ++st->clock; changed = true; }
    if (g->was_in != in) { g->was_in = in; changed = true; }
    if (!in) return changed;

    char fallback[BN_KEY_LEN];
    snprintf(fallback, sizeof(fallback), "%s#%u", incubator_id, (unsigned)g->gen);
    if (stay_id && stay_id[0]) {
        /* ThingsBoard empieza a mandar stay_id con el bebe dentro: hereda lo del contador */
        bn_entry_t *f = find(st, fallback);
        if (f && !find(st, stay_id)) {
            snprintf(f->key, sizeof(f->key), "%s", stay_id);
            changed = true;
        }
        snprintf(key_out, BN_KEY_LEN, "%s", stay_id);
    } else {
        snprintf(key_out, BN_KEY_LEN, "%s", fallback);
    }
    return changed;
}

const char *bn_name(const bn_store_t *st, const char *key)
{
    bn_entry_t *e = find((bn_store_t *)st, key);
    return e ? e->name : "";
}

bool bn_should_ask(const bn_store_t *st, const char *key)
{
    if (!key || !key[0]) return false;
    bn_entry_t *e = find((bn_store_t *)st, key);
    return !e || (!e->asked && !e->name[0]);
}

bool bn_mark_asked(bn_store_t *st, const char *key)
{
    if (!key || !key[0]) return false;
    bn_entry_t *e = find(st, key);
    if (e && e->asked) return false;
    e = slot(st, key);
    e->asked = true;
    e->used = ++st->clock;
    return true;
}

bool bn_set_name(bn_store_t *st, const char *key, const char *raw)
{
    char clean[BN_NAME_BYTES];
    if (!key || !key[0] || !bn_clean_name(raw, clean)) return false;
    bn_entry_t *e = slot(st, key);
    snprintf(e->name, sizeof(e->name), "%s", clean);
    e->asked = true;
    e->used = ++st->clock;
    return true;
}

/* Longitud en bytes del caracter UTF-8 que empieza en `c` (1 si el byte no es valido). */
static int u8_len(unsigned char c)
{
    if (c >= 0xF0) return 4;
    if (c >= 0xE0) return 3;
    if (c >= 0xC0) return 2;
    return 1;
}

bool bn_clean_name(const char *raw, char out[BN_NAME_BYTES])
{
    out[0] = '\0';
    if (!raw) return false;
    while (*raw == ' ') raw++;
    size_t n = 0;
    int chars = 0;
    while (raw[n] && chars < BN_NAME_MAX_CHARS) {
        int len = u8_len((unsigned char)raw[n]);
        for (int k = 1; k < len; k++) {
            if (!raw[n + k]) { len = k; break; } /* UTF-8 cortado: se queda fuera */
        }
        if (n + (size_t)len > BN_NAME_BYTES - 1) break;
        n += (size_t)len;
        chars++;
    }
    while (n > 0 && raw[n - 1] == ' ') n--;
    memcpy(out, raw, n);
    out[n] = '\0';
    return n > 0;
}
