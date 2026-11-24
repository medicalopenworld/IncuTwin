#include "event_ring.h"

void event_ring_push(event_ring_t *r, const event_entry_t *e)
{
    r->e[r->head] = *e;
    r->head = (uint8_t)((r->head + 1) % EVENT_LOG_MAX);
    if (r->count < EVENT_LOG_MAX) r->count++;
    if (r->unseen < EVENT_LOG_MAX) r->unseen++;
}

bool event_ring_get(const event_ring_t *r, uint8_t i, event_entry_t *out)
{
    if (i >= r->count) return false;
    *out = r->e[(r->head + 2 * EVENT_LOG_MAX - 1 - i) % EVENT_LOG_MAX];
    return true;
}

void event_ring_mark_seen(event_ring_t *r)
{
    r->unseen = 0;
}
