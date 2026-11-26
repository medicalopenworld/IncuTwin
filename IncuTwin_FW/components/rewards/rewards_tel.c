#include "rewards_tel.h"

#include <stdio.h>

#define TEL_BITS_OF(T) ((int)(sizeof(T) * 8))

static const rw_country_t *find_cc(const rw_state_t *s, const char iso[2])
{
    int n = s->n_countries < RW_PASSPORT_MAX ? s->n_countries : RW_PASSPORT_MAX;
    for (int i = 0; i < n; i++)
        if (s->passport[i].iso[0] == iso[0] && s->passport[i].iso[1] == iso[1]) return &s->passport[i];
    return NULL;
}

void tel_rewards_diff(const rw_state_t *b, const rw_state_t *a, const rw_news_t *news,
                      const char *why, rw_tel_sink_t sink, void *ctx)
{
    (void)news;
    if (!b || !a || !sink) return;
    char d[96];

    if (a->earned > b->earned) {
        snprintf(d, sizeof d, "{\"n\":%u,\"why\":\"%s\",\"bal\":%u}", (unsigned)(a->earned - b->earned),
                 why ? why : "", (unsigned)a->balance);
        sink("heart", d, ctx);
    }

    /* Anchura de seals/unlocked tomada del propio campo: si rw_state_t los ensancha (la tienda pasa
     * unlocked a uint64_t), el diff sigue viendo todos los bits sin truncar ni desplazar de mas. */
    typedef __typeof__(a->seals) seal_bits_t;
    seal_bits_t new_seals = (seal_bits_t)(a->seals & ~b->seals);
    for (int bit = 0; bit < TEL_BITS_OF(seal_bits_t) && bit < RW_SEAL_COUNT; bit++) {
        if (!(new_seals & ((seal_bits_t)1 << bit))) continue;
        snprintf(d, sizeof d, "{\"id\":%d}", bit);
        sink("seal", d, ctx);
    }

    if (rw_level(a->earned) > rw_level(b->earned)) {
        snprintf(d, sizeof d, "{\"level\":%d}", (int)rw_level(a->earned));
        sink("level", d, ctx);
    }

    const char *ev = NULL;
    if (a->streak > b->streak) ev = "up";
    else if (a->streak <= 1 && b->streak > 1) ev = "broken";
    if (ev) {
        snprintf(d, sizeof d, "{\"ev\":\"%s\",\"n\":%u}", ev, (unsigned)a->streak);
        sink("streak", d, ctx);
    }
    if (a->saved < b->saved) {
        snprintf(d, sizeof d, "{\"ev\":\"saved\",\"n\":%u}", (unsigned)a->streak);
        sink("streak", d, ctx);
    }

    int na = a->n_countries < RW_PASSPORT_MAX ? a->n_countries : RW_PASSPORT_MAX;
    for (int i = 0; i < na; i++) {
        const rw_country_t *old = find_cc(b, a->passport[i].iso);
        if (old && !(a->passport[i].star && !old->star)) continue;
        snprintf(d, sizeof d, "{\"cc\":\"%c%c\",\"star\":%d}", a->passport[i].iso[0], a->passport[i].iso[1],
                 a->passport[i].star ? 1 : 0);
        sink("passport", d, ctx);
    }

    typedef __typeof__(a->unlocked) item_bits_t;
    item_bits_t fresh = (item_bits_t)(a->unlocked & ~b->unlocked);
    if (a->balance < b->balance && a->earned <= b->earned && fresh) {
        for (int bit = 0; bit < TEL_BITS_OF(item_bits_t) && bit < RW_ITEM_COUNT; bit++) {
            if (!(fresh & ((item_bits_t)1 << bit))) continue;
            snprintf(d, sizeof d, "{\"item\":%d,\"price\":%u,\"bal\":%u}", bit,
                     (unsigned)rw_price((rw_item_t)bit), (unsigned)a->balance);
            sink("spend", d, ctx);
        }
    }
}
