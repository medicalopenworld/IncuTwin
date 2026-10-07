#include "backoff.h"

void backoff_init(backoff_t *b, uint32_t min_s, uint32_t max_s)
{
    b->min_s = min_s ? min_s : 1;
    b->max_s = max_s > b->min_s ? max_s : b->min_s;
    b->next_s = b->min_s;
}

void backoff_reset(backoff_t *b) { b->next_s = b->min_s; }

uint32_t backoff_next_nominal(backoff_t *b)
{
    uint32_t cur = b->next_s;
    uint32_t nxt = cur * 2;
    b->next_s = nxt > b->max_s ? b->max_s : nxt;
    return cur;
}

uint32_t backoff_apply_jitter_ms(uint32_t nominal_s, uint32_t rnd)
{
    /* jitter en [-20 %, +20 %] del nominal */
    uint32_t span_ms = nominal_s * 1000 * 2 / 10; /* 20 % */
    uint32_t base_ms = nominal_s * 1000 - span_ms;
    return base_ms + (uint32_t)((uint64_t)rnd * (2 * span_ms + 1) / 0x100000000ULL);
}
