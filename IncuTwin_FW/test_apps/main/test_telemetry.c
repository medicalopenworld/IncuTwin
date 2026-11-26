/* Telemetria: eventos, lotes, diff de recompensas y filtro de pantallas (spec telemetry-events). */
#include <stdio.h>
#include <string.h>

#include "rewards_tel.h"
#include "tel_logic.h"
#include "unity.h"

static tel_event_t EV(uint32_t seq, int64_t t, uint32_t up, const char *k, const char *d)
{
    tel_event_t e = { .seq = seq, .t = t, .up = up };
    strncpy(e.kind, k, sizeof e.kind - 1);
    strncpy(e.d, d, sizeof e.d - 1);
    return e;
}

TEST_CASE("tel: evento codificado", "[telemetry]") {
    tel_event_t e = EV(7, 0, 100, "boot", "{\"reason\":\"poweron\",\"fw\":\"2.1.0\"}");
    char out[200];
    int n = tel_event_encode(out, sizeof out, &e);
    const char *want = "{\"s\":7,\"t\":0,\"up\":100,\"k\":\"boot\",\"d\":{\"reason\":\"poweron\",\"fw\":\"2.1.0\"}}";
    TEST_ASSERT_EQUAL_STRING(want, out);
    TEST_ASSERT_EQUAL_INT((int)strlen(want), n);
}

TEST_CASE("tel: evento sin cabida da -1", "[telemetry]") {
    tel_event_t e = EV(7, 0, 100, "boot", "{}");
    char out[10];
    TEST_ASSERT_EQUAL_INT(-1, tel_event_encode(out, sizeof out, &e));
}

TEST_CASE("tel: t=0 antes de hora y t>0 despues", "[telemetry]") {
    tel_event_t a = EV(1, 0, 3, "boot", "{}");
    tel_event_t b = EV(2, 1790000000LL, 9, "screen", "{\"p\":1}");
    char out[120];
    tel_event_encode(out, sizeof out, &a);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"t\":0,\"up\":3"));
    tel_event_encode(out, sizeof out, &b);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"t\":1790000000,\"up\":9"));
}

TEST_CASE("tel: lote de 2", "[telemetry]") {
    tel_event_t ev[2] = {
        EV(1031, 1790000000, 55, "gesture", "{\"sec\":12,\"inc\":\"353\",\"super\":0}"),
        EV(1032, 1790000000, 55, "heart", "{\"n\":1,\"why\":\"gesture\",\"bal\":24}"),
    };
    char out[TEL_BATCH_MAX + 1];
    int used = 0;
    int n = tel_batch_build(out, sizeof out, 1, 42, ev, 2, &used);
    const char *want =
        "{\"gen\":1,\"b\":42,\"e\":["
        "{\"s\":1031,\"t\":1790000000,\"up\":55,\"k\":\"gesture\",\"d\":{\"sec\":12,\"inc\":\"353\",\"super\":0}},"
        "{\"s\":1032,\"t\":1790000000,\"up\":55,\"k\":\"heart\",\"d\":{\"n\":1,\"why\":\"gesture\",\"bal\":24}}]}";
    TEST_ASSERT_EQUAL_INT(2, used);
    TEST_ASSERT_EQUAL_STRING(want, out);
    TEST_ASSERT_EQUAL_INT((int)strlen(want), n);
}

TEST_CASE("tel: lote parte a 20", "[telemetry]") {
    static tel_event_t ev[25];
    for (int i = 0; i < 25; i++) ev[i] = EV(100 + i, 0, i, "screen", "{\"p\":1}");
    static char out[TEL_BATCH_MAX + 1];
    int used = 0;
    TEST_ASSERT_GREATER_THAN(0, tel_batch_build(out, sizeof out, 1, 1, ev, 25, &used));
    TEST_ASSERT_EQUAL_INT(20, used);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"s\":119,"));
    TEST_ASSERT_NULL(strstr(out, "\"s\":120,"));
}

TEST_CASE("tel: lote parte por tamano", "[telemetry]") {
    char big[TEL_D_MAX];
    memset(big, 'x', sizeof big);
    big[0] = '{'; big[TEL_D_MAX - 2] = '}'; big[TEL_D_MAX - 1] = '\0';
    static tel_event_t ev[20];
    for (int i = 0; i < 20; i++) ev[i] = EV(5000 + i, 1790000000, 99999, "screen", big);
    static char out[TEL_BATCH_MAX + 64];
    int used = 0;
    int n = tel_batch_build(out, sizeof out, 1, 1, ev, 20, &used);
    TEST_ASSERT_GREATER_THAN(0, used);
    TEST_ASSERT_LESS_THAN(20, used);
    TEST_ASSERT_LESS_OR_EQUAL(TEL_BATCH_MAX, n);
    TEST_ASSERT_EQUAL_INT((int)strlen(out), n);
    TEST_ASSERT_EQUAL_CHAR('}', out[n - 1]);
}

/* --- diff de recompensas --- */

typedef struct { int n; char kind[8][12]; char d[8][96]; } rec_t;

static void sink_rec(const char *k, const char *d, void *ctx)
{
    rec_t *r = ctx;
    if (r->n >= 8) return;
    strncpy(r->kind[r->n], k, 11);
    strncpy(r->d[r->n], d, 95);
    r->n++;
}

TEST_CASE("tel: diff de gesto con sello y nivel", "[telemetry]") {
    rw_state_t b, a; rw_init(&b); a = b;
    b.earned = 24; b.balance = 24;
    a.earned = 25; a.balance = 25;
    a.seals |= (1u << RW_SEAL_FIRST);
    rec_t r = { 0 };
    tel_rewards_diff(&b, &a, NULL, "gesture", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(3, r.n);
    TEST_ASSERT_EQUAL_STRING("heart", r.kind[0]);
    TEST_ASSERT_EQUAL_STRING("{\"n\":1,\"why\":\"gesture\",\"bal\":25}", r.d[0]);
    TEST_ASSERT_EQUAL_STRING("seal", r.kind[1]);
    TEST_ASSERT_EQUAL_STRING("{\"id\":0}", r.d[1]);
    TEST_ASSERT_EQUAL_STRING("level", r.kind[2]);
    TEST_ASSERT_EQUAL_STRING("{\"level\":1}", r.d[2]);
}

TEST_CASE("tel: diff sin cambios no emite nada", "[telemetry]") {
    rw_state_t b; rw_init(&b);
    rec_t r = { 0 };
    tel_rewards_diff(&b, &b, NULL, "gesture", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(0, r.n);
}

TEST_CASE("tel: diff racha rota y abrazo guardado", "[telemetry]") {
    rw_state_t b, a; rw_init(&b); a = b;
    b.streak = 5; b.saved = 1;
    a.streak = 1; a.saved = 0;
    rec_t r = { 0 };
    tel_rewards_diff(&b, &a, NULL, "gesture", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(2, r.n);
    TEST_ASSERT_EQUAL_STRING("streak", r.kind[0]);
    TEST_ASSERT_EQUAL_STRING("{\"ev\":\"broken\",\"n\":1}", r.d[0]);
    TEST_ASSERT_EQUAL_STRING("streak", r.kind[1]);
    TEST_ASSERT_EQUAL_STRING("{\"ev\":\"saved\",\"n\":1}", r.d[1]);
}

TEST_CASE("tel: diff racha sube", "[telemetry]") {
    rw_state_t b, a; rw_init(&b); a = b;
    b.streak = 3; a.streak = 4;
    rec_t r = { 0 };
    tel_rewards_diff(&b, &a, NULL, "gesture", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(1, r.n);
    TEST_ASSERT_EQUAL_STRING("{\"ev\":\"up\",\"n\":4}", r.d[0]);
}

TEST_CASE("tel: diff compra", "[telemetry]") {
    rw_state_t b, a; rw_init(&b); a = b;
    b.earned = 40; b.balance = 40;
    a.earned = 40; a.balance = 25;
    a.unlocked |= (1u << RW_ITEM_HALO_LAVENDER);
    rec_t r = { 0 };
    tel_rewards_diff(&b, &a, NULL, "buy", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(1, r.n);
    TEST_ASSERT_EQUAL_STRING("spend", r.kind[0]);
    TEST_ASSERT_EQUAL_STRING("{\"item\":1,\"price\":15,\"bal\":25}", r.d[0]);
}

/* El diff toma la anchura de unlocked/seals del propio campo: el ultimo articulo y el ultimo sello
 * validos se ven sea cual sea el tipo (la tienda ensancha unlocked a uint64_t con ~26 articulos). */
TEST_CASE("tel: diff compra del ultimo articulo y ultimo sello", "[telemetry]") {
    rw_state_t b, a; rw_init(&b); a = b;
    b.earned = 900; b.balance = 900;
    a.earned = 900; a.balance = 10;
    const int top = RW_ITEM_COUNT - 1;
    a.unlocked |= (__typeof__(a.unlocked))((__typeof__(a.unlocked))1 << top);
    rec_t r = { 0 };
    tel_rewards_diff(&b, &a, NULL, "buy", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(1, r.n);
    TEST_ASSERT_EQUAL_STRING("spend", r.kind[0]);
    char want[64];
    snprintf(want, sizeof want, "{\"item\":%d,\"price\":%u,\"bal\":10}", top, (unsigned)rw_price((rw_item_t)top));
    TEST_ASSERT_EQUAL_STRING(want, r.d[0]);

    rw_state_t c = b;
    c.seals |= (__typeof__(c.seals))((__typeof__(c.seals))1 << (RW_SEAL_COUNT - 1));
    memset(&r, 0, sizeof r);
    tel_rewards_diff(&b, &c, NULL, "home", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(1, r.n);
    TEST_ASSERT_EQUAL_STRING("seal", r.kind[0]);
    snprintf(want, sizeof want, "{\"id\":%d}", RW_SEAL_COUNT - 1);
    TEST_ASSERT_EQUAL_STRING(want, r.d[0]);
}

TEST_CASE("tel: diff apuesta sin desbloqueo no es compra", "[telemetry]") {
    rw_state_t b, a; rw_init(&b); a = b;
    b.earned = 40; b.balance = 40;
    a.earned = 40; a.balance = 37;
    rec_t r = { 0 };
    tel_rewards_diff(&b, &a, NULL, "incupray", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(0, r.n);
}

TEST_CASE("tel: diff pais nuevo con estrella", "[telemetry]") {
    rw_state_t b, a; rw_init(&b); a = b;
    a.n_countries = 1;
    a.passport[0] = (rw_country_t){ .iso = { 'T', 'G' }, .star = 1 };
    rec_t r = { 0 };
    tel_rewards_diff(&b, &a, NULL, "home", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(1, r.n);
    TEST_ASSERT_EQUAL_STRING("passport", r.kind[0]);
    TEST_ASSERT_EQUAL_STRING("{\"cc\":\"TG\",\"star\":1}", r.d[0]);

    /* mismo pais sin estrella -> con estrella: otro evento; sin cambios: nada */
    b = a; b.passport[0].star = 0;
    memset(&r, 0, sizeof r);
    tel_rewards_diff(&b, &a, NULL, "home", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(1, r.n);
    memset(&r, 0, sizeof r);
    tel_rewards_diff(&a, &a, NULL, "home", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(0, r.n);
}

/* --- filtro de pantallas --- */

TEST_CASE("tel: pantalla repetida en < 5 s no se registra; otra pantalla si", "[telemetry]") {
    int last_page = -1;
    uint32_t last_ms = 0;
    TEST_ASSERT_TRUE(tel_screen_should_log(2, 1000, &last_page, &last_ms));
    TEST_ASSERT_FALSE(tel_screen_should_log(2, 3000, &last_page, &last_ms));
    TEST_ASSERT_FALSE(tel_screen_should_log(2, 5999, &last_page, &last_ms));
    TEST_ASSERT_TRUE(tel_screen_should_log(3, 6000, &last_page, &last_ms)); /* otra */
    TEST_ASSERT_TRUE(tel_screen_should_log(2, 6100, &last_page, &last_ms)); /* vuelve: otra respecto a la ultima */
    TEST_ASSERT_TRUE(tel_screen_should_log(2, 11100, &last_page, &last_ms)); /* misma pero pasaron 5 s */
}
