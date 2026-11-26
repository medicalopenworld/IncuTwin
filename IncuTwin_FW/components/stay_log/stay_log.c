#include "stay_log.h"

#include <string.h>

static bool same(const char *a, const char *b) { return strncmp(a, b, TWIN_INCUBATOR_ID_LEN) == 0; }

/* FNV-1a; nunca 0 (0 = sin stay_id) */
static uint32_t key_of(const char *stay_id)
{
    if (!stay_id || !stay_id[0]) return 0;
    uint32_t h = 2166136261u;
    for (const char *p = stay_id; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    return h ? h : 1;
}

static stay_t *at(stay_log_t *l, uint8_t i) /* 0 = la mas reciente */
{
    return &l->s[(l->head + 2 * STAY_LOG_MAX - 1 - i) % STAY_LOG_MAX];
}

static stay_t *open_of(stay_log_t *l, const char *inc)
{
    for (uint8_t i = 0; i < l->count; i++) {
        stay_t *s = at(l, i);
        if (s->open && same(s->inc, inc)) return s;
    }
    return NULL;
}

/* La de esta IncuNest; si no esta, un hueco libre o la que lleva mas tiempo sin cambios */
static stay_inc_t *inc_of(stay_log_t *l, const char *inc)
{
    stay_inc_t *free_slot = NULL, *oldest = NULL;
    for (int i = 0; i < STAY_INC_MAX; i++) {
        stay_inc_t *e = &l->inc[i];
        if (e->inc[0] && same(e->inc, inc)) return e;
        if (!e->inc[0] && !free_slot) free_slot = e;
        if (e->inc[0] && (!oldest || e->order < oldest->order)) oldest = e;
    }
    stay_inc_t *e = free_slot ? free_slot : oldest;
    memset(e, 0, sizeof(*e));
    strncpy(e->inc, inc, sizeof(e->inc) - 1);
    return e;
}

static void touch(stay_log_t *l, stay_inc_t *e, const stay_obs_t *o)
{
    if (o->country && o->country[0]) strncpy(e->country, o->country, sizeof(e->country) - 1);
    if (o->now) e->last_at = o->now;
    e->order = ++l->seq;
}

static void close_stay(stay_t *s, uint32_t now)
{
    s->open = 0;
    s->left_at = now;
}

bool stay_log_observe(stay_log_t *l, const stay_obs_t *o)
{
    if (!o->inc || !o->inc[0]) return false;
    bool changed = false;
    uint32_t key = key_of(o->stay_id);
    stay_t *open = open_of(l, o->inc);

    if (open && !open->arrived_at && o->now) { /* llego sin hora: la de ahora, casi la misma */
        open->arrived_at = o->now;
        changed = true;
    }
    if (o->present) {
        if (open && key && open->key && open->key != key) { /* otro stay_id: otro bebe */
            close_stay(open, o->now);
            open = NULL;
            changed = true;
        }
        if (open && key && !open->key) { /* ThingsBoard empieza a mandar stay_id: es el mismo */
            open->key = key;
            changed = true;
        }
        if (!open) {
            stay_t *s = &l->s[l->head];
            memset(s, 0, sizeof(*s));
            s->arrived_at = o->now;
            s->key = key;
            s->sex = o->sex;
            s->open = 1;
            strncpy(s->inc, o->inc, sizeof(s->inc) - 1);
            l->head = (uint8_t)((l->head + 1) % STAY_LOG_MAX);
            if (l->count < STAY_LOG_MAX) l->count++;
            stay_inc_t *e = inc_of(l, o->inc);
            if (e->babies < UINT16_MAX) e->babies++;
            touch(l, e, o);
            return true;
        }
        if (o->sex != TWIN_SEX_UNKNOWN && open->sex != o->sex) { /* el sexo puede llegar despues */
            open->sex = o->sex;
            changed = true;
        }
    } else if (open && o->known) {
        close_stay(open, o->now);
        touch(l, inc_of(l, o->inc), o);
        return true;
    }
    if (changed) {
        stay_inc_t *e = inc_of(l, o->inc);
        if (o->country && o->country[0] && strncmp(e->country, o->country, sizeof(e->country)) != 0) {
            strncpy(e->country, o->country, sizeof(e->country) - 1);
        }
    }
    return changed;
}

uint8_t stay_log_incubators(const stay_log_t *l, uint8_t out[STAY_INC_MAX])
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < STAY_INC_MAX; i++) {
        if (!l->inc[i].inc[0]) continue;
        /* insercion por `order` descendente (pocos elementos) */
        uint8_t j = n++;
        while (j > 0 && l->inc[out[j - 1]].order < l->inc[i].order) {
            out[j] = out[j - 1];
            j--;
        }
        out[j] = i;
    }
    return n;
}

uint32_t stay_log_open_key(const stay_log_t *l, const char *inc)
{
    if (!inc || !inc[0]) return 0;
    for (uint8_t i = 0; i < l->count; i++) {
        const stay_t *s = &l->s[(l->head + 2 * STAY_LOG_MAX - 1 - i) % STAY_LOG_MAX];
        if (!s->open || !same(s->inc, inc)) continue;
        uint32_t h = key_of(inc) ^ (s->key ? s->key : s->arrived_at * 2654435761u);
        return h ? h : 1;
    }
    return 0;
}

uint8_t stay_log_babies(const stay_log_t *l, const char *inc, const stay_t *out[], uint8_t max)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < l->count && n < max; i++) {
        const stay_t *s = &l->s[(l->head + 2 * STAY_LOG_MAX - 1 - i) % STAY_LOG_MAX];
        if (same(s->inc, inc)) out[n++] = s;
    }
    return n;
}
