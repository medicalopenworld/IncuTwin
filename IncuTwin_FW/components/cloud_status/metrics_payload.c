#include "metrics_payload.h"

#include <stdbool.h>
#include <stdio.h>

/* Copia s a dst solo si es [A-Za-z0-9._-]; si no (o NULL), dst queda vacio. Asi no rompe el JSON. */
static void safe_str(char *dst, size_t cap, const char *s)
{
    size_t i = 0;
    for (; s && s[i]; i++) {
        char ch = s[i];
        bool ok = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
                  ch == '.' || ch == '_' || ch == '-';
        if (!ok || i + 1 >= cap) { dst[0] = '\0'; return; }
        dst[i] = ch;
    }
    dst[i] = '\0';
}

int metrics_payload_build(char *out, size_t out_len, const metrics_usage_t *u, const rw_state_t *r,
                          const metrics_extra_t *x)
{
    char fw[48], inc[24], phase[16];
    safe_str(fw, sizeof(fw), x->fw);
    safe_str(inc, sizeof(inc), x->inc);
    safe_str(phase, sizeof(phase), x->phase);
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
                     "\"stars\":%u,\"unlocked\":%u,\"halo\":%u,\"bg\":%u,\"garland\":%u,\"named_incunest\":%u,"
                     "\"gen\":%lu,\"fw\":\"%s\",\"inc\":\"%s\",\"phase\":\"%s\",\"lang\":%u,\"vol\":%u,"
                     "\"bright\":%u,\"demo_n\":%lu,\"boot_n\":%lu,\"q\":%lu,\"lost\":%lu}",
                     (long long)u->ts, (unsigned long)u->on_s, (unsigned long)u->conn_s, (unsigned long)u->hand_s,
                     (unsigned long)u->hand_n, r->balance, (unsigned long)r->earned, (int)rw_level(r->earned),
                     r->streak, r->saved, r->total_days, r->babies, r->seals, __builtin_popcount(r->seals), cc,
                     r->n_countries, stars, r->unlocked, r->halo, r->bg, r->garland_on,
                     (r->unlocked & (1u << RW_ITEM_NAMED_INCUNEST)) ? 1u : 0u, (unsigned long)x->gen, fw, inc, phase,
                     (unsigned)x->lang, (unsigned)x->vol, (unsigned)x->bright, (unsigned long)x->demo_n,
                     (unsigned long)x->boot_n, (unsigned long)x->q, (unsigned long)x->lost);
    return (n < 0 || (size_t)n >= out_len) ? -1 : n;
}

int redeem_payload_build(char *out, size_t out_len, int64_t ts)
{
    int n = snprintf(out, out_len, "{\"redeem\":\"named_incunest\",\"hearts\":%u,\"ts\":%lld}",
                     rw_price(RW_ITEM_NAMED_INCUNEST), (long long)ts);
    return (n < 0 || (size_t)n >= out_len) ? -1 : n;
}
