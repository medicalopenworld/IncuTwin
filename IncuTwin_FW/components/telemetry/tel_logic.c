#include "tel_logic.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------- JSON estricto */

#define TEL_JSON_DEPTH 4

typedef struct { const char *p, *end; } jc_t;

static void j_ws(jc_t *c)
{
    while (c->p < c->end && (*c->p == ' ' || *c->p == '\t' || *c->p == '\n' || *c->p == '\r')) c->p++;
}

static bool j_lit(jc_t *c, const char *w)
{
    size_t n = strlen(w);
    if ((size_t)(c->end - c->p) < n || memcmp(c->p, w, n) != 0) return false;
    c->p += n;
    return true;
}

static bool j_str(jc_t *c)
{
    if (c->p >= c->end || *c->p != '"') return false;
    c->p++;
    while (c->p < c->end) {
        unsigned char ch = (unsigned char)*c->p++;
        if (ch == '"') return true;
        if (ch < 0x20) return false;
        if (ch == '\\') {
            if (c->p >= c->end) return false;
            char e = *c->p++;
            if (e == 'u') {
                for (int i = 0; i < 4; i++) {
                    if (c->p >= c->end) return false;
                    char h = *c->p++;
                    if (!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F'))) return false;
                }
            } else if (!strchr("\"\\/bfnrt", e) || e == '\0') {
                return false;
            }
        }
    }
    return false;
}

static bool j_digits(jc_t *c)
{
    const char *s = c->p;
    while (c->p < c->end && *c->p >= '0' && *c->p <= '9') c->p++;
    return c->p > s;
}

static bool j_num(jc_t *c)
{
    if (c->p < c->end && *c->p == '-') c->p++;
    if (c->p >= c->end) return false;
    if (*c->p == '0') c->p++;
    else if (*c->p >= '1' && *c->p <= '9') j_digits(c);
    else return false;
    if (c->p < c->end && *c->p == '.') {
        c->p++;
        if (!j_digits(c)) return false;
    }
    if (c->p < c->end && (*c->p == 'e' || *c->p == 'E')) {
        c->p++;
        if (c->p < c->end && (*c->p == '+' || *c->p == '-')) c->p++;
        if (!j_digits(c)) return false;
    }
    return true;
}

static bool j_val(jc_t *c, int depth)
{
    j_ws(c);
    if (c->p >= c->end) return false;
    char ch = *c->p;
    if (ch == '"') return j_str(c);
    if (ch == 't') return j_lit(c, "true");
    if (ch == 'f') return j_lit(c, "false");
    if (ch == 'n') return j_lit(c, "null");
    if (ch == '-' || (ch >= '0' && ch <= '9')) return j_num(c);
    if (ch != '{' && ch != '[') return false;
    if (depth >= TEL_JSON_DEPTH) return false;
    bool obj = ch == '{';
    char close = obj ? '}' : ']';
    c->p++;
    j_ws(c);
    if (c->p < c->end && *c->p == close) {
        c->p++;
        return true;
    }
    for (;;) {
        j_ws(c);
        if (obj) {
            if (!j_str(c)) return false;
            j_ws(c);
            if (c->p >= c->end || *c->p++ != ':') return false;
        }
        if (!j_val(c, depth + 1)) return false;
        j_ws(c);
        if (c->p >= c->end) return false;
        char t = *c->p++;
        if (t == close) return true;
        if (t != ',') return false;
    }
}

bool tel_json_object_ok(const char *s, size_t max_len)
{
    if (!s) return false;
    size_t n = strnlen(s, max_len);
    if (n < 2 || n >= max_len || s[0] != '{') return false;
    jc_t c = { s, s + n };
    if (!j_val(&c, 0)) return false;
    j_ws(&c);
    return c.p == c.end;
}

int tel_event_encode(char *out, size_t len, const tel_event_t *e)
{
    if (!out || !len || !e) return -1;
    const char *d = tel_json_object_ok(e->d, sizeof e->d) ? e->d : "{}";
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

bool tel_ids_floor(uint32_t *gen, uint32_t *boot, uint32_t q_gen, uint32_t q_boot)
{
    bool changed = false;
    if (gen && q_gen > *gen) {
        *gen = q_gen;
        changed = true;
    }
    if (boot && q_boot != UINT32_MAX && q_boot + 1 > *boot) {
        *boot = q_boot + 1;
        changed = true;
    }
    return changed;
}
