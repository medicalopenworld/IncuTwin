#include "acc_logic.h"

#include <string.h>

/* Diferencias en uint32_t: correctas aunque el reloj desborde. */
static inline uint32_t since(uint32_t now, uint32_t t) { return now - t; }

uint32_t acc_hash(const char *s)
{
    uint32_t h = 2166136261u;
    for (; s && *s; s++) {
        h ^= (uint8_t)*s;
        h *= 16777619u;
    }
    return h;
}

static void copy_id(char dst[TWIN_INCUBATOR_ID_LEN], const char *src)
{
    strncpy(dst, src ? src : "", TWIN_INCUBATOR_ID_LEN - 1);
    dst[TWIN_INCUBATOR_ID_LEN - 1] = '\0';
}

void acc_init(acc_t *a, uint32_t seed)
{
    memset(a, 0, sizeof(*a));
    a->p.version = ACC_PERSIST_VERSION;
    a->p.phase = TWIN_ACC_SEARCHING;
    a->p.gesture_day = ACC_NO_DAY;
    a->p.mask_day = ACC_NO_DAY;
    a->p.remind_day = ACC_NO_DAY;
    a->p.farewell_day = ACC_NO_DAY;
    a->seed = seed;
}

void acc_restore(acc_t *a, const acc_persist_t *p, uint32_t seed, uint32_t now_s)
{
    bool ok = p && p->version == ACC_PERSIST_VERSION && p->phase >= TWIN_ACC_SEARCHING &&
              p->phase <= TWIN_ACC_FAREWELL && p->n_hist <= ACC_HIST_MAX &&
              memchr(p->cur, '\0', sizeof(p->cur)) != NULL;
    for (int i = 0; ok && i < p->n_hist; i++) {
        ok = memchr(p->hist[i].id, '\0', sizeof(p->hist[i].id)) != NULL;
    }
    acc_init(a, seed);
    if (!ok) return;
    a->p = *p;
    if (!a->p.cur[0]) a->p.phase = TWIN_ACC_SEARCHING;
    a->ending_since = now_s; /* tras reiniciar, el margen vuelve a empezar */
}

void acc_hold_pick(acc_t *a, uint32_t until_s) { a->pick_after = until_s; }

int acc_seen_index(const acc_t *a, const char *id)
{
    for (int i = 0; i < a->n_seen; i++) {
        if (!strcmp(a->seen[i].id, id)) return i;
    }
    return -1;
}

static int hist_index(const acc_t *a, const char *id)
{
    for (int i = 0; i < a->p.n_hist; i++) {
        if (!strcmp(a->p.hist[i].id, id)) return i;
    }
    return -1;
}

uint32_t acc_hist_secs(const acc_t *a, const char *id)
{
    int i = hist_index(a, id);
    return i < 0 ? 0 : a->p.hist[i].secs;
}

/* Entrada del historial para la actual; si esta lleno, sale la de menos tiempo (nunca la actual,
 * que es la que entra). */
static acc_hist_t *hist_slot(acc_t *a, const char *id)
{
    int i = hist_index(a, id);
    if (i >= 0) return &a->p.hist[i];
    if (a->p.n_hist < ACC_HIST_MAX) {
        i = a->p.n_hist++;
    } else {
        i = 0;
        for (int k = 1; k < ACC_HIST_MAX; k++) {
            if (a->p.hist[k].secs < a->p.hist[i].secs) i = k;
        }
    }
    copy_id(a->p.hist[i].id, id);
    a->p.hist[i].secs = 0;
    return &a->p.hist[i];
}

/* Entre las IncuNest con bebe: mas historial; empate, hash por panel. */
static int best_candidate(const acc_t *a)
{
    int best = -1;
    uint32_t best_secs = 0, best_tie = 0;
    for (int i = 0; i < a->n_seen; i++) {
        const acc_seen_t *s = &a->seen[i];
        if (!s->present || s->home) continue;
        uint32_t secs = acc_hist_secs(a, s->id);
        uint32_t tie = acc_hash(s->id) ^ a->seed;
        if (best < 0 || secs > best_secs || (secs == best_secs && tie > best_tie)) {
            best = i;
            best_secs = secs;
            best_tie = tie;
        }
    }
    return best;
}

/* Buscando: elige (o se queda sin ninguna). Devuelve lo que ha cambiado. */
static uint32_t pick(acc_t *a)
{
    int b = best_candidate(a);
    if (b < 0) {
        bool had = a->p.cur[0] != '\0';
        bool phase_changed = a->p.phase != TWIN_ACC_SEARCHING;
        a->p.cur[0] = '\0';
        a->p.phase = TWIN_ACC_SEARCHING;
        a->p.engaged = 0;
        return (had ? ACC_R_SWITCH : 0) | (had || phase_changed ? ACC_R_PHASE | ACC_R_SAVE : 0);
    }
    copy_id(a->p.cur, a->seen[b].id);
    a->p.phase = TWIN_ACC_WAITING;
    a->p.engaged = 0;
    return ACC_R_SWITCH | ACC_R_PHASE | ACC_R_SAVE;
}

static uint32_t search(acc_t *a, uint32_t now_s)
{
    a->p.phase = TWIN_ACC_SEARCHING;
    if (since(now_s, a->pick_after) > 0x7fffffffu) { /* aun en la espera de los retenidos */
        bool had = a->p.cur[0] != '\0';
        a->p.cur[0] = '\0';
        a->p.engaged = 0;
        return (had ? ACC_R_SWITCH : 0) | ACC_R_PHASE | ACC_R_SAVE;
    }
    return pick(a) | ACC_R_PHASE | ACC_R_SAVE;
}

/* Entrada de la tabla para `id` (la crea si hace falta); -1 si esta llena de bebes. */
static int seen_slot(acc_t *a, const char *id)
{
    int i = acc_seen_index(a, id);
    if (i >= 0) return i;
    if (a->n_seen < ACC_MAX_INC) {
        i = a->n_seen++;
    } else { /* llena: reutilizar una sin bebe que no sea la actual */
        for (int k = 0; k < ACC_MAX_INC && i < 0; k++) {
            if (!a->seen[k].present && strcmp(a->seen[k].id, a->p.cur) != 0) i = k;
        }
        if (i < 0) return -1;
    }
    copy_id(a->seen[i].id, id);
    a->seen[i].present = false;
    a->seen[i].home = false;
    return i;
}

bool acc_country_ok(const char *cc)
{
    if (!cc || strlen(cc) != 2) return false;
    if (cc[0] < 'A' || cc[0] > 'Z' || cc[1] < 'A' || cc[1] > 'Z') return false;
    return strcmp(cc, "ES") != 0 && strcmp(cc, "XX") != 0;
}

uint32_t acc_exclude(acc_t *a, const char *id, uint32_t now_s)
{
    if (!id || !id[0]) return 0;
    int i = seen_slot(a, id);
    if (i >= 0) {
        a->seen[i].present = false;
        a->seen[i].home = false;
    }
    if (a->p.cur[0] && !strcmp(id, a->p.cur)) return search(a, now_s);
    return 0;
}

uint32_t acc_seen_update(acc_t *a, const char *id, bool present, bool home, uint32_t now_s)
{
    if (!id || !id[0]) return 0;
    int i = seen_slot(a, id);
    if (i < 0) return 0;
    a->seen[i].present = present;
    a->seen[i].home = home;

    uint32_t r = 0;
    if (a->p.cur[0] && !strcmp(id, a->p.cur)) {
        twin_acc_t ph = (twin_acc_t)a->p.phase;
        if (ph != TWIN_ACC_FAREWELL) r |= ACC_R_FORWARD; /* el "home" aun se aplica */
        switch (ph) {
        case TWIN_ACC_WAITING:
        case TWIN_ACC_WITH:
        case TWIN_ACC_ENDING:
            if (home) {
                a->p.phase = TWIN_ACC_FAREWELL;
                a->p.farewell_day = ACC_NO_DAY; /* lo fija el siguiente tick */
                r |= ACC_R_PHASE | ACC_R_SAVE;
            } else if (!present && ph != TWIN_ACC_ENDING) {
                a->p.phase = TWIN_ACC_ENDING;
                a->ending_since = now_s;
                r |= ACC_R_PHASE | ACC_R_SAVE;
            } else if (present && ph == TWIN_ACC_ENDING) {
                a->p.phase = a->p.engaged ? TWIN_ACC_WITH : TWIN_ACC_WAITING;
                r |= ACC_R_PHASE | ACC_R_SAVE;
            }
            break;
        default:
            break;
        }
    }
    if (a->p.phase == TWIN_ACC_SEARCHING && since(now_s, a->pick_after) <= 0x7fffffffu) {
        uint32_t pr = pick(a);
        r |= pr;
        if ((pr & ACC_R_SWITCH) && !strcmp(id, a->p.cur)) r |= ACC_R_FORWARD;
    }
    return r;
}

static void mark_day(acc_t *a, int32_t day)
{
    if (day == ACC_NO_DAY) return;
    if (a->p.mask_day == ACC_NO_DAY) {
        a->p.mask_day = day;
        a->p.days_mask = 0;
        a->p.super_mask = 0;
    } else if (day > a->p.mask_day) {
        int32_t diff = day - a->p.mask_day;
        a->p.days_mask = diff >= 7 ? 0 : (uint8_t)((a->p.days_mask << diff) & 0x7F);
        a->p.super_mask = diff >= 7 ? 0 : (uint8_t)((a->p.super_mask << diff) & 0x7F);
        a->p.mask_day = day;
    }
    int32_t off = a->p.mask_day - day; /* un dia anterior (reloj corregido) cae en su bit */
    if (off >= 0 && off < 7) a->p.days_mask |= (uint8_t)(1u << off);
}

uint32_t acc_gesture(acc_t *a, int32_t day)
{
    twin_acc_t ph = (twin_acc_t)a->p.phase;
    if (ph != TWIN_ACC_WAITING && ph != TWIN_ACC_WITH) return 0;
    a->p.engaged = 1;
    a->p.phase = TWIN_ACC_WITH;
    a->p.gesture_day = day;
    mark_day(a, day);
    return ACC_R_PHASE | ACC_R_SAVE;
}

uint32_t acc_super(acc_t *a, int32_t day)
{
    uint32_t r = acc_gesture(a, day);
    if (!r || day == ACC_NO_DAY) return r;
    int32_t off = a->p.mask_day - day;
    if (off >= 0 && off < 7) a->p.super_mask |= (uint8_t)(1u << off);
    return r;
}

static uint8_t shifted(uint8_t mask, int32_t mask_day, int32_t day)
{
    if (mask_day == ACC_NO_DAY) return 0;
    if (day == ACC_NO_DAY || day <= mask_day) return mask;
    int32_t diff = day - mask_day;
    return diff >= 7 ? 0 : (uint8_t)((mask << diff) & 0x7F);
}

uint8_t acc_super_mask(const acc_t *a, int32_t day) { return shifted(a->p.super_mask, a->p.mask_day, day); }

uint32_t acc_touch(acc_t *a, uint32_t now_s)
{
    if (a->p.phase != TWIN_ACC_FAREWELL) return 0;
    return search(a, now_s);
}

uint32_t acc_tick(acc_t *a, uint32_t now_s, int32_t day, int hour, bool shown)
{
    uint32_t r = 0;
    twin_acc_t ph = (twin_acc_t)a->p.phase;
    /* pasada la espera de los retenidos sin noticias de la actual (desaparecio, o no tenia
     * retenido y se ha reiniciado): es un fin de estancia, o se quedaria esperandola siempre */
    if ((ph == TWIN_ACC_WAITING || ph == TWIN_ACC_WITH) && a->p.cur[0] &&
        since(now_s, a->pick_after) <= 0x7fffffffu && acc_seen_index(a, a->p.cur) < 0) {
        a->p.phase = TWIN_ACC_ENDING;
        a->ending_since = now_s;
        r |= ACC_R_PHASE | ACC_R_SAVE;
    }
    switch ((twin_acc_t)a->p.phase) {
    case TWIN_ACC_SEARCHING:
        if (since(now_s, a->pick_after) <= 0x7fffffffu) r |= pick(a);
        break;
    case TWIN_ACC_ENDING:
        if (since(now_s, a->ending_since) >= ACC_ENDING_S) r |= search(a, now_s);
        break;
    case TWIN_ACC_FAREWELL:
        if (day == ACC_NO_DAY) break;
        if (a->p.farewell_day == ACC_NO_DAY) {
            a->p.farewell_day = day;
            r |= ACC_R_SAVE;
        } else if (day != a->p.farewell_day) {
            r |= search(a, now_s);
        }
        break;
    case TWIN_ACC_WITH:
        if (shown && a->p.cur[0] && a->p.gesture_day == day) {
            hist_slot(a, a->p.cur)->secs++;
            if (++a->unsaved_s >= ACC_SAVE_S) {
                a->unsaved_s = 0;
                r |= ACC_R_SAVE;
            }
        }
        break;
    default:
        break;
    }
    ph = (twin_acc_t)a->p.phase;
    if (day != ACC_NO_DAY && hour >= ACC_REMIND_HOUR && (ph == TWIN_ACC_WAITING || ph == TWIN_ACC_WITH) &&
        a->p.gesture_day != day && a->p.remind_day != day) {
        a->p.remind_day = day;
        r |= ACC_R_REMIND | ACC_R_PHASE | ACC_R_SAVE;
    }
    return r;
}

bool acc_remind_active(const acc_t *a, int32_t day, int hour)
{
    twin_acc_t ph = (twin_acc_t)a->p.phase;
    return day != ACC_NO_DAY && hour >= ACC_REMIND_HOUR && (ph == TWIN_ACC_WAITING || ph == TWIN_ACC_WITH) &&
           a->p.gesture_day != day;
}

uint8_t acc_week_mask(const acc_t *a, int32_t day) { return shifted(a->p.days_mask, a->p.mask_day, day); }

acc_streak_msg_t acc_streak_msg(uint8_t mask, int *n)
{
    mask &= 0x7F;
    bool today = mask & 1u;
    int run = 0;
    for (int b = today ? 0 : 1; b < 7 && (mask & (1u << b)); b++) run++;
    if (n) *n = run;
    if (!mask) return ACC_SM_NONE;
    if (today) return run >= 7 ? ACC_SM_WEEK : run >= 2 ? ACC_SM_RUN : ACC_SM_TODAY1;
    if (run == 0) return ACC_SM_START;
    return run == 1 ? ACC_SM_KEEP1 : ACC_SM_KEEP;
}

bool acc_quiet(int hour)
{
    return hour >= 0 && (hour >= ACC_QUIET_FROM || hour < ACC_QUIET_TO);
}
