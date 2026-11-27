#include "affection.h"

#include <string.h>

/* Diferencias en uint32_t: correctas aunque el reloj desborde. */
static inline uint32_t since(uint32_t now, uint32_t t) { return now - t; }

static inline int iabs(int v) { return v < 0 ? -v : v; }

bool aff_open(rw_level_t level, aff_kind_t kind)
{
    switch (kind) {
    case AFF_NONE:    return true;
    case AFF_CARESS:  return level >= RW_LEVEL_CARESS;
    case AFF_TICKLES: return level >= RW_LEVEL_TICKLES;
    default:          return false;
    }
}

/* Lo reconocido, recortado a lo que el nivel deja hacer. */
static aff_kind_t capped(const aff_stroke_t *s, rw_level_t level)
{
    for (aff_kind_t k = s->best; k > AFF_NONE; k--) {
        if (aff_open(level, k)) return k;
    }
    return AFF_NONE;
}

static void raise(aff_stroke_t *s, aff_kind_t k)
{
    if (k > s->best) s->best = k;
}

void aff_down(aff_stroke_t *s, int16_t x, int16_t y, uint32_t now_ms)
{
    (void)now_ms;
    memset(s, 0, sizeof(*s));
    s->down = true;
    s->x = x;
    s->y = y;
}

void aff_up(aff_stroke_t *s)
{
    s->down = false;
}

aff_kind_t aff_move(aff_stroke_t *s, int16_t x, int16_t y, uint32_t now_ms, rw_level_t level)
{
    if (!s->down) return AFF_NONE;

    int dx = (int)x - s->x;
    int dy = (int)y - s->y;
    int step = iabs(dx) + iabs(dy);
    if (step < AFF_STEP_MIN_PX) return capped(s, level); /* ruido del tactil: no cuenta */

    /* Sentido horizontal del tramo; un tramo mas vertical que horizontal no lo cambia. */
    int d = iabs(dx) >= iabs(dy) ? (dx > 0) - (dx < 0) : 0;
    if (d != 0 && s->dir != 0 && d != s->dir) {
        /* Sentido contrario: hasta que el tramo nuevo no llega a TURN_MIN no es un cambio
         * (el punto contado se queda en el extremo). */
        if (iabs(dx) < AFF_TURN_MIN_PX) return capped(s, level);
        memmove(&s->turn_ms[0], &s->turn_ms[1], sizeof(s->turn_ms) - sizeof(s->turn_ms[0]));
        s->turn_ms[3] = now_ms;
        if (s->turns < AFF_TICKLE_TURNS) s->turns++;
        if (s->turns >= AFF_TICKLE_TURNS && since(now_ms, s->turn_ms[0]) <= AFF_TICKLE_WINDOW_MS)
            raise(s, AFF_TICKLES);
    }
    if (d != 0) s->dir = (int8_t)d;

    uint32_t path = (uint32_t)s->path + (uint32_t)step;
    s->path = path > UINT16_MAX ? UINT16_MAX : (uint16_t)path;
    s->x = x;
    s->y = y;
    if (s->path >= AFF_CARESS_PX) raise(s, AFF_CARESS);
    return capped(s, level);
}
