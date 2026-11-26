#include "metrics_payload.h"

#include <stdio.h>

int metrics_payload_build(char *out, size_t out_len, const metrics_usage_t *u, const rw_state_t *r)
{
    char cc[RW_PASSPORT_MAX * 3 + 1];
    size_t k = 0;
    unsigned stars = 0;
    for (int i = 0; i < r->n_countries && i < RW_PASSPORT_MAX; i++) {
        if (k) cc[k++] = ',';
        cc[k++] = r->passport[i].iso[0];
        cc[k++] = r->passport[i].iso[1];
        stars += r->passport[i].star ? 1u : 0u;
    }
    cc[k] = '\0';
    int n = snprintf(out, out_len,
                     "{\"ts\":%lld,\"on_s\":%lu,\"conn_s\":%lu,\"hand_s\":%lu,\"hand_n\":%lu,"
                     "\"hearts\":%u,\"earned\":%lu,\"level\":%d,\"streak\":%u,\"saved\":%u,\"days\":%u,"
                     "\"babies\":%u,\"seals\":%u,\"seals_n\":%d,\"countries\":\"%s\",\"countries_n\":%u,"
                     "\"stars\":%u,\"unlocked\":%u,\"halo\":%u,\"bg\":%u,\"garland\":%u,\"named_incunest\":%u}",
                     (long long)u->ts, (unsigned long)u->on_s, (unsigned long)u->conn_s, (unsigned long)u->hand_s,
                     (unsigned long)u->hand_n, r->balance, (unsigned long)r->earned, (int)rw_level(r->earned),
                     r->streak, r->saved, r->total_days, r->babies, r->seals, __builtin_popcount(r->seals), cc,
                     r->n_countries, stars, r->unlocked, r->halo, r->bg, r->garland_on,
                     (r->unlocked & (1u << RW_ITEM_NAMED_INCUNEST)) ? 1u : 0u);
    return (n < 0 || (size_t)n >= out_len) ? -1 : n;
}

int redeem_payload_build(char *out, size_t out_len, int64_t ts)
{
    int n = snprintf(out, out_len, "{\"redeem\":\"named_incunest\",\"hearts\":%u,\"ts\":%lld}",
                     rw_price(RW_ITEM_NAMED_INCUNEST), (long long)ts);
    return (n < 0 || (size_t)n >= out_len) ? -1 : n;
}
