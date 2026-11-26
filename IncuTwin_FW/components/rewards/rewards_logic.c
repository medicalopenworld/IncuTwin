#include "rewards_logic.h"

#include <string.h>

static const uint32_t LEVEL_MIN[RW_LEVEL_COUNT] = { 0, 25, 75, 150, 300 };

/* Precio en corazones; 0 = de serie (o la guirnalda, que no se compra) */
static const uint16_t PRICE[RW_ITEM_COUNT] = {
    [RW_ITEM_HALO_SKY] = 0,  [RW_ITEM_HALO_LAVENDER] = 15, [RW_ITEM_HALO_MINT] = 15,
    [RW_ITEM_HALO_PEACH] = 20, [RW_ITEM_HALO_LILAC] = 30,
    [RW_ITEM_BG_DEFAULT] = 0, [RW_ITEM_BG_CREAM] = 10, [RW_ITEM_BG_PINK] = 20, [RW_ITEM_BG_AQUA] = 20,
    [RW_ITEM_GARLAND] = 0,
    [RW_ITEM_NAMED_INCUNEST] = 365, /* un año de corazones */
    [RW_ITEM_HAT_BOY_BASIC] = 0, [RW_ITEM_HAT_BOY_MINT] = 15, [RW_ITEM_HAT_BOY_CHICK] = 15,
    [RW_ITEM_HAT_BOY_CLOUD] = 20,
    [RW_ITEM_HAT_GIRL_BASIC] = 0, [RW_ITEM_HAT_GIRL_LILAC] = 15, [RW_ITEM_HAT_GIRL_PEACH] = 15,
    [RW_ITEM_HAT_GIRL_MINT] = 20,
    [RW_ITEM_BLANKET_BASIC] = 0, [RW_ITEM_BLANKET_CREAM] = 15, [RW_ITEM_BLANKET_MINT] = 15,
    [RW_ITEM_BLANKET_LAVENDER] = 20, [RW_ITEM_BLANKET_PEACH] = 20,
    [RW_ITEM_SANTA] = 10, [RW_ITEM_SNOW] = 10,
};
_Static_assert(RW_ITEM_COUNT <= 64, "unlocked es de 64 bits");
_Static_assert(sizeof(rw_state_v1_t) == RW_STATE_V1_SIZE, "el guardado v1 no puede cambiar");

/* De serie: lo que cualquier IncuTwin tiene sin comprar */
#define RW_STOCK (RW_BIT(RW_ITEM_HALO_SKY) | RW_BIT(RW_ITEM_BG_DEFAULT) | RW_BIT(RW_ITEM_HAT_BOY_BASIC) |                   RW_BIT(RW_ITEM_HAT_GIRL_BASIC) | RW_BIT(RW_ITEM_BLANKET_BASIC))

#define DEC_DAYS_FOR_GARLAND 10
#define BIT(x) (1u << (x))

void rw_init(rw_state_t *s)
{
    memset(s, 0, sizeof(*s));
    s->version = RW_VERSION;
    s->last_day = RW_DAY_NONE;
    s->super_day = RW_DAY_NONE;
    s->unlocked = RW_STOCK;
    s->halo = RW_ITEM_HALO_SKY;
    s->bg = RW_ITEM_BG_DEFAULT;
    s->hat_boy = RW_ITEM_HAT_BOY_BASIC;
    s->hat_girl = RW_ITEM_HAT_GIRL_BASIC;
    s->blanket = RW_ITEM_BLANKET_BASIC;
}

bool rw_migrate_v1(rw_state_t *out, const rw_state_v1_t *in)
{
    if (in->version != 1 || in->n_countries > RW_PASSPORT_MAX) return false;
    rw_init(out);
    out->saved = in->saved;
    out->seals = in->seals;
    out->n_countries = in->n_countries;
    out->balance = in->balance;
    out->streak = in->streak;
    out->total_days = in->total_days;
    out->babies = in->babies;
    out->dec_year = in->dec_year;
    out->unlocked = RW_STOCK | (uint64_t)in->unlocked; /* los bits de v1 valen igual en v2 */
    out->earned = in->earned;
    out->last_day = in->last_day;
    out->super_day = in->super_day;
    out->dec_days = in->dec_days;
    out->last_stay = in->last_stay;
    out->last_stay_cc[0] = in->last_stay_cc[0];
    out->last_stay_cc[1] = in->last_stay_cc[1];
    out->last_stay_home = in->last_stay_home;
    out->halo = in->halo <= RW_HALO_LAST ? in->halo : RW_ITEM_HALO_SKY;
    out->bg = in->bg >= RW_BG_FIRST && in->bg <= RW_BG_LAST ? in->bg : RW_ITEM_BG_DEFAULT;
    out->garland_on = in->garland_on;
    out->redeem_sent = in->redeem_sent;
    memcpy(out->passport, in->passport, sizeof(out->passport));
    return true;
}

/* Howard Hinnant, days_from_civil */
int32_t rw_day_of(int year, int month, int mday)
{
    int y = year - (month <= 2);
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + mday - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int32_t)(era * 146097 + (int)doe - 719468);
}

rw_level_t rw_level(uint32_t earned)
{
    rw_level_t l = RW_LEVEL_SPARK;
    for (int i = 0; i < RW_LEVEL_COUNT; i++) {
        if (earned >= LEVEL_MIN[i]) l = (rw_level_t)i;
    }
    return l;
}

uint32_t rw_level_min(rw_level_t level) { return level < RW_LEVEL_COUNT ? LEVEL_MIN[level] : 0; }

uint16_t rw_price(rw_item_t item) { return item < RW_ITEM_COUNT ? PRICE[item] : 0; }

bool rw_xmas_season(int month, int mday)
{
    return (month == 12 && mday >= 24) || (month == 1 && mday >= 1 && mday <= 6);
}

bool rw_is_xmas(rw_item_t item) { return item == RW_ITEM_SANTA || item == RW_ITEM_SNOW || item == RW_ITEM_GARLAND; }

static bool owns(const rw_state_t *s, rw_item_t i) { return i < RW_ITEM_COUNT && (s->unlocked & RW_BIT(i)); }

bool rw_wears(const rw_state_t *s, rw_item_t item, int month, int mday)
{
    if (!rw_xmas_season(month, mday) || !owns(s, item)) return false;
    switch (item) {
    case RW_ITEM_SANTA:   return s->santa_on;
    case RW_ITEM_SNOW:    return s->snow_on;
    case RW_ITEM_GARLAND: return s->garland_on;
    default:              return false;
    }
}

rw_item_t rw_hat_for(const rw_state_t *s, bool girl, int month, int mday)
{
    if (rw_wears(s, RW_ITEM_SANTA, month, mday)) return RW_ITEM_SANTA;
    rw_item_t h = (rw_item_t)(girl ? s->hat_girl : s->hat_boy);
    if (girl) return h >= RW_HAT_GIRL_FIRST && h <= RW_HAT_GIRL_LAST ? h : RW_ITEM_HAT_GIRL_BASIC;
    return h >= RW_HAT_BOY_FIRST && h <= RW_HAT_BOY_LAST ? h : RW_ITEM_HAT_BOY_BASIC;
}

/* Suma corazones y anota si se sube de nivel */
static void earn(rw_state_t *s, rw_news_t *n, uint8_t hearts)
{
    if (!hearts) return;
    rw_level_t before = rw_level(s->earned);
    s->balance = (uint16_t)(s->balance + hearts < UINT16_MAX ? s->balance + hearts : UINT16_MAX);
    s->earned += hearts;
    n->hearts += hearts;
    if (rw_level(s->earned) != before) n->level_up = true;
}

static void seal(rw_state_t *s, rw_news_t *n, rw_seal_t which)
{
    if (s->seals & BIT(which)) return;
    s->seals |= (uint8_t)BIT(which);
    n->new_seals |= (uint8_t)BIT(which);
}

static rw_country_t *country_of(rw_state_t *s, const char *cc)
{
    for (int i = 0; i < s->n_countries; i++) {
        if (s->passport[i].iso[0] == cc[0] && s->passport[i].iso[1] == cc[1]) return &s->passport[i];
    }
    return NULL;
}

static void passport_add(rw_state_t *s, const char *cc)
{
    if (!cc || !cc[0] || !cc[1] || country_of(s, cc) || s->n_countries >= RW_PASSPORT_MAX) return;
    rw_country_t *c = &s->passport[s->n_countries++];
    c->iso[0] = cc[0];
    c->iso[1] = cc[1];
    c->star = 0;
}

/* Racha: hoy tras el ultimo dia con gesto. Los dias de en medio que fueron de descanso no cuentan;
 * cada uno que no lo fue gasta un abrazo guardado; sin abrazos, la racha vuelve a empezar. */
static void streak_step(rw_state_t *s, const rw_gesture_t *g)
{
    if (s->last_day == RW_DAY_NONE || g->day <= s->last_day) {
        s->streak = 1;
        return;
    }
    bool broken = false;
    for (int32_t d = s->last_day + 1; d < g->day && !broken; d++) {
        int32_t ago = g->day - d;
        if (ago <= 6 && (g->rest_mask & BIT(ago))) continue;
        if (s->saved > 0) {
            s->saved--;
        } else {
            broken = true;
        }
    }
    s->streak = broken ? 1 : (uint16_t)(s->streak + 1);
}

rw_news_t rw_on_gesture(rw_state_t *s, const rw_gesture_t *g)
{
    rw_news_t n = { 0 };
    if (!g->time_valid) return n;

    if (g->day != s->last_day) { /* primer gesto del dia */
        streak_step(s, g);
        s->last_day = g->day;
        if (s->total_days < UINT16_MAX) s->total_days++;
        uint8_t hearts = 1;
        if (s->streak % 7 == 0) {
            hearts++;
            if (s->saved < RW_SAVED_MAX) s->saved++;
        }
        earn(s, &n, hearts);
        seal(s, &n, RW_SEAL_FIRST);
        if (s->streak >= 7) seal(s, &n, RW_SEAL_WEEK);
        if (s->streak >= 30) seal(s, &n, RW_SEAL_MONTH_RUN);
        if (s->total_days >= 30) seal(s, &n, RW_SEAL_30_DAYS);

        if (g->month == 12 && g->mday >= 1 && g->mday <= 31) {
            if (s->dec_year != (uint16_t)g->year) {
                s->dec_year = (uint16_t)g->year;
                s->dec_days = 0;
            }
            s->dec_days |= BIT(g->mday);
            if (__builtin_popcount(s->dec_days) >= DEC_DAYS_FOR_GARLAND && !(s->unlocked & BIT(RW_ITEM_GARLAND))) {
                s->unlocked |= RW_BIT(RW_ITEM_GARLAND);
                s->garland_on = 1;
                n.garland = true;
            }
        }
    }

    if (g->stay && g->stay != s->last_stay) { /* otro bebe */
        s->last_stay = g->stay;
        s->last_stay_home = 0;
        s->last_stay_cc[0] = g->country && g->country[0] ? g->country[0] : 0;
        s->last_stay_cc[1] = g->country && g->country[0] ? g->country[1] : 0;
        if (s->babies < UINT16_MAX) s->babies++;
        if (s->babies >= 3) seal(s, &n, RW_SEAL_3_BABIES);
    }
    passport_add(s, g->country);
    return n;
}

rw_news_t rw_on_super(rw_state_t *s, bool time_valid, int32_t day)
{
    rw_news_t n = { 0 };
    if (!time_valid || s->super_day == day) return n;
    s->super_day = day;
    earn(s, &n, 2);
    seal(s, &n, RW_SEAL_SUPER);
    return n;
}

rw_news_t rw_on_home(rw_state_t *s, uint32_t stay)
{
    rw_news_t n = { 0 };
    if (!stay || stay != s->last_stay || s->last_stay_home) return n;
    s->last_stay_home = 1;
    seal(s, &n, RW_SEAL_HOME);
    rw_country_t *c = s->last_stay_cc[0] ? country_of(s, s->last_stay_cc) : NULL;
    if (c) c->star = 1;
    return n;
}

rw_news_t rw_add(rw_state_t *s, uint8_t hearts)
{
    rw_news_t n = { 0 };
    earn(s, &n, hearts);
    return n;
}

bool rw_spend(rw_state_t *s, uint16_t n)
{
    if (s->balance < n) return false;
    s->balance = (uint16_t)(s->balance - n);
    return true;
}

rw_unlock_t rw_unlock(rw_state_t *s, rw_item_t item, int month, int mday, uint16_t *missing)
{
    if (missing) *missing = 0;
    if (item >= RW_ITEM_COUNT) return RW_UNLOCK_NOT_FOR_SALE;
    if (item == RW_ITEM_NAMED_INCUNEST && owns(s, item)) return RW_UNLOCK_DONE; /* una sola vez */
    if (rw_is_xmas(item) && !rw_xmas_season(month, mday)) return RW_UNLOCK_OFF_SEASON;
    if (!owns(s, item)) {
        if (item == RW_ITEM_GARLAND) return RW_UNLOCK_NOT_FOR_SALE; /* solo con diciembre */
        uint16_t price = PRICE[item];
        if (s->balance < price) {
            if (missing) *missing = (uint16_t)(price - s->balance);
            return RW_UNLOCK_NO_FUNDS;
        }
        s->balance = (uint16_t)(s->balance - price);
        s->unlocked |= RW_BIT(item);
        if (item == RW_ITEM_SANTA) s->santa_on = 0; /* al comprarlo queda puesto (se alterna abajo) */
        if (item == RW_ITEM_SNOW) s->snow_on = 0;
    }
    if (item <= RW_HALO_LAST) s->halo = (uint8_t)item;
    else if (item >= RW_BG_FIRST && item <= RW_BG_LAST) s->bg = (uint8_t)item;
    else if (item == RW_ITEM_GARLAND) s->garland_on = !s->garland_on;
    else if (item == RW_ITEM_SANTA) s->santa_on = !s->santa_on;
    else if (item == RW_ITEM_SNOW) s->snow_on = !s->snow_on;
    else if (item >= RW_HAT_BOY_FIRST && item <= RW_HAT_BOY_LAST) {
        s->hat_boy = (uint8_t)item;
        s->santa_on = 0; /* elegir un gorro quita Papa Noel */
    } else if (item >= RW_HAT_GIRL_FIRST && item <= RW_HAT_GIRL_LAST) {
        s->hat_girl = (uint8_t)item;
        s->santa_on = 0;
    } else if (item >= RW_BLANKET_FIRST && item <= RW_BLANKET_LAST) {
        s->blanket = (uint8_t)item;
    }
    return RW_UNLOCK_OK;
}

void rw_seed_streak(rw_state_t *s, uint8_t week, uint8_t rest, int32_t today)
{
    if (s->streak || s->last_day != RW_DAY_NONE) return;
    int k = (week & 1u) ? 0 : 1; /* hoy aun puede llegar */
    int32_t last = RW_DAY_NONE;
    uint16_t run = 0;
    for (; k < 7; k++) {
        if (week & BIT(k)) {
            if (last == RW_DAY_NONE) last = today - k;
            run++;
        } else if (!(rest & BIT(k))) {
            break;
        }
    }
    if (!run) return;
    s->streak = run;
    s->last_day = last;
}

void rw_demo_seed(rw_state_t *s)
{
    rw_init(s);
    s->balance = 50; /* que en la demo se pueda comprar de todo (la tienda empieza en 10) */
    s->earned = 12;
    s->seals = (uint8_t)(BIT(RW_SEAL_FIRST) | BIT(RW_SEAL_WEEK));
    s->n_countries = 1;
    s->passport[0] = (rw_country_t){ { 'C', 'M' }, 0 };
}
