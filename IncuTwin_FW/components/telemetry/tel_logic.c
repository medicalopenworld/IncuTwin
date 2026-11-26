#include "tel_logic.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int tel_event_encode(char *out, size_t len, const tel_event_t *e)
{
    if (!out || !len || !e) return -1;
    const char *d = e->d[0] ? e->d : "{}";
    int n = snprintf(out, len, "{\"s\":%" PRIu32 ",\"t\":%" PRId64 ",\"up\":%" PRIu32 ",\"k\":\"%.*s\",\"d\":%.*s}",
                     e->seq, e->t, e->up, TEL_KIND_MAX - 1, e->kind, TEL_D_MAX - 1, d);
    if (n < 0 || (size_t)n >= len) {
        out[0] = '\0';
        return -1;
    }
    return n;
}

int tel_batch_build(char *out, size_t len, uint32_t gen, uint32_t boot,
                    const tel_event_t *ev, int n, int *used)
{
    if (used) *used = 0;
    if (!out || !len || !ev || n <= 0) return -1;
    size_t cap = len < TEL_BATCH_MAX + 1 ? len : TEL_BATCH_MAX + 1; /* con el NUL */
    int pos = snprintf(out, cap, "{\"gen\":%" PRIu32 ",\"b\":%" PRIu32 ",\"e\":[", gen, boot);
    if (pos < 0 || (size_t)pos + 3 > cap) {
        out[0] = '\0';
        return -1;
    }
    int count = 0;
    for (int i = 0; i < n && count < TEL_BATCH_N; i++) {
        size_t sep = count ? 1 : 0;
        size_t room = cap - (size_t)pos - sep;
        if (room < 4) break;
        int w = tel_event_encode(out + pos + sep, room - 2, &ev[i]); /* deja sitio a "]}" */
        if (w < 0) break;
        if (sep) out[pos] = ',';
        pos += (int)sep + w;
        count++;
    }
    if (!count) {
        out[0] = '\0';
        return -1;
    }
    out[pos++] = ']';
    out[pos++] = '}';
    out[pos] = '\0';
    if (used) *used = count;
    return pos;
}

bool tel_screen_should_log(int page, uint32_t now_ms, int *last_page, uint32_t *last_ms)
{
    if (*last_page == page && (uint32_t)(now_ms - *last_ms) < TEL_SCREEN_DEDUP_MS) return false;
    *last_page = page;
    *last_ms = now_ms;
    return true;
}
