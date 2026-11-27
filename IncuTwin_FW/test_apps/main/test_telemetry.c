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
    big[0] = '{'; big[1] = 0x22; big[2] = 'p'; big[3] = 0x22; big[4] = ':'; big[5] = 0x22; big[TEL_D_MAX - 3] = 0x22; big[TEL_D_MAX - 2] = '}'; big[TEL_D_MAX - 1] = '\0';
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
    b.earned = 9; b.balance = 9;
    a.earned = 10; a.balance = 10;
    a.seals |= (1u << RW_SEAL_FIRST);
    rec_t r = { 0 };
    tel_rewards_diff(&b, &a, NULL, "gesture", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(3, r.n);
    TEST_ASSERT_EQUAL_STRING("heart", r.kind[0]);
    TEST_ASSERT_EQUAL_STRING("{\"n\":1,\"why\":\"gesture\",\"bal\":10}", r.d[0]);
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

/* Una compra real = exactamente un `spend`: elegir lo ya comprado, poner/quitar Navidad o la
 * guirnalda no gastan, y el canje de la IncuNest con nombre sale con su precio. */
TEST_CASE("tel: una compra emite un solo spend; elegir o alternar no emite", "[telemetry]") {
    rw_state_t s; rw_init(&s);
    s.earned = 2000; s.balance = 2000;
    rec_t r = { 0 };
    rw_state_t before = s;
    TEST_ASSERT_EQUAL_INT(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_HAT_BOY_MINT, 0, 0, NULL));
    tel_rewards_diff(&before, &s, NULL, "buy", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(1, r.n);
    TEST_ASSERT_EQUAL_STRING("spend", r.kind[0]);
    char want[64];
    snprintf(want, sizeof want, "{\"item\":%d,\"price\":%u,\"bal\":%u}", RW_ITEM_HAT_BOY_MINT,
             (unsigned)rw_price(RW_ITEM_HAT_BOY_MINT), (unsigned)s.balance);
    TEST_ASSERT_EQUAL_STRING(want, r.d[0]);
    TEST_ASSERT_EQUAL_UINT16(2000 - rw_price(RW_ITEM_HAT_BOY_MINT), s.balance);

    /* elegir otra vez lo ya suyo */
    before = s; memset(&r, 0, sizeof r);
    TEST_ASSERT_EQUAL_INT(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_HAT_BOY_MINT, 0, 0, NULL));
    tel_rewards_diff(&before, &s, NULL, "buy", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(0, r.n);

    /* Papa Noel y copos: comprar (en temporada) gasta una vez; alternar no gasta */
    before = s; memset(&r, 0, sizeof r);
    TEST_ASSERT_EQUAL_INT(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_SANTA, 12, 25, NULL));
    tel_rewards_diff(&before, &s, NULL, "buy", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(1, r.n);
    TEST_ASSERT_EQUAL_STRING("spend", r.kind[0]);
    TEST_ASSERT_EQUAL_INT(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_SNOW, 12, 25, NULL));
    s.unlocked |= RW_BIT(RW_ITEM_GARLAND); /* la guirnalda es gratis: se consigue con diciembre */
    const rw_item_t toggles[] = { RW_ITEM_SANTA, RW_ITEM_SNOW, RW_ITEM_GARLAND };
    for (unsigned i = 0; i < sizeof toggles / sizeof toggles[0]; i++) {
        for (int k = 0; k < 2; k++) {
            before = s; memset(&r, 0, sizeof r);
            TEST_ASSERT_EQUAL_INT(RW_UNLOCK_OK, rw_unlock(&s, toggles[i], 12, 25, NULL));
            tel_rewards_diff(&before, &s, NULL, "buy", sink_rec, &r);
            TEST_ASSERT_EQUAL_INT(0, r.n);
        }
    }

    /* la IncuNest con tu nombre */
    before = s; memset(&r, 0, sizeof r);
    TEST_ASSERT_EQUAL_INT(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_NAMED_INCUNEST, 0, 0, NULL));
    tel_rewards_diff(&before, &s, NULL, "buy", sink_rec, &r);
    TEST_ASSERT_EQUAL_INT(1, r.n);
    TEST_ASSERT_EQUAL_STRING("spend", r.kind[0]);
    TEST_ASSERT_EQUAL_UINT16(365, rw_price(RW_ITEM_NAMED_INCUNEST));
    snprintf(want, sizeof want, "{\"item\":%d,\"price\":365,\"bal\":%u}", RW_ITEM_NAMED_INCUNEST,
             (unsigned)s.balance);
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

/* --- JSON estricto --- */

TEST_CASE("tel: tel_json_object_ok acepta objetos validos", "[telemetry]") {
    TEST_ASSERT_TRUE(tel_json_object_ok("{}", TEL_D_MAX));
    TEST_ASSERT_TRUE(tel_json_object_ok("{\"a\":1}", TEL_D_MAX));
    TEST_ASSERT_TRUE(tel_json_object_ok("{\"a\":-1.5e3,\"b\":[1,2,{\"c\":null}],\"s\":\"x\\\"y\\u00e9\"}", TEL_D_MAX));
    TEST_ASSERT_TRUE(tel_json_object_ok("{ \"a\" : [ true , false ] , \"a\" : 0 } ", TEL_D_MAX));
    TEST_ASSERT_TRUE(tel_json_object_ok("{\"s\":\"\\/\\b\\f\\n\\r\\t\\\\\"}", TEL_D_MAX));
    TEST_ASSERT_TRUE(tel_json_object_ok("{\"a\":{\"b\":{\"c\":{\"d\":1}}}}", TEL_D_MAX)); /* profundidad 4 */
}

TEST_CASE("tel: tel_json_object_ok rechaza lo invalido", "[telemetry]") {
    const char *bad[] = {
        "{\"i\":00}", "{\"i\":01}", "{\"a\":1,}", "{\"a\"}", "{a:1}", "{\"a\":1}x", "[1]", "\"x\"",
        "{\"a\":\"abc}", "{\"a\":\"\\q\"}", "{\"a\":{\"b\":{\"c\":{\"d\":{\"e\":1}}}}}",
        "{\"a\":\"x\ty\"}", "{\"a\":+1}", "{\"a\":.5}", "{\"a\":1.}", "{\"a\":NaN}", "", "{", "{\"a\":1",
        "{\"a\":\"\\u12G4\"}", "{\"a\":-}", "{\"a\":1e}", "{\"a\":tru}",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        TEST_ASSERT_FALSE_MESSAGE(tel_json_object_ok(bad[i], TEL_D_MAX), bad[i]);
    }
    TEST_ASSERT_FALSE(tel_json_object_ok(NULL, TEL_D_MAX));
    /* demasiado largo: 159 caracteres caben, 160 no */
    char big[TEL_D_MAX + 8];
    memset(big, ' ', sizeof big);
    big[sizeof big - 1] = '\0';
    big[0] = '{';
    big[158] = '}';
    big[159] = '\0';
    TEST_ASSERT_TRUE(tel_json_object_ok(big, TEL_D_MAX));
    big[159] = ' ';
    big[159] = '}';
    big[158] = ' ';
    big[160] = '\0';
    TEST_ASSERT_FALSE(tel_json_object_ok(big, TEL_D_MAX));
}

TEST_CASE("tel: un d con JSON invalido sale {} y el lote sigue valido", "[telemetry]") {
    tel_event_t ev[3] = {
        EV(1, 0, 1, "boot", "{\"a\":1}"),
        EV(2, 0, 1, "boot", "{\"i\":00,\"pad\":\"xx\"}"),
        EV(3, 0, 1, "boot", ""),
    };
    char out[TEL_BATCH_MAX + 1];
    int used = 0;
    int n = tel_batch_build(out, sizeof out, 1, 1, ev, 3, &used);
    TEST_ASSERT_EQUAL_INT(3, used);
    TEST_ASSERT_EQUAL_INT((int)strlen(out), n);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"s\":2,\"t\":0,\"up\":1,\"k\":\"boot\",\"d\":{}}"));
    TEST_ASSERT_NULL(strstr(out, "00"));
    TEST_ASSERT_TRUE(tel_json_object_ok(out, TEL_BATCH_MAX + 1));
}
