/* Payloads de metricas y canje (metrics_payload, spec device-metrics). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "metrics_payload.h"
#include "unity.h"

static const metrics_extra_t X_BASE = { .gen = 1, .boot_n = 5, .demo_n = 0, .q = 3, .lost = 0,
                                        .fw = "1.2.3", .inc = "353", .phase = "with", .lang = 0, .vol = 2, .bright = 80 };

TEST_CASE("metrics: uso, corazones y pasaporte", "[metrics]") {
    rw_state_t r; rw_init(&r);
    r.balance = 23; r.earned = 30; r.streak = 9; r.saved = 1; r.total_days = 12; r.babies = 2;
    r.seals = 0x03; r.n_countries = 2;
    r.passport[0] = (rw_country_t){ { 'T', 'G' }, 1 };
    r.passport[1] = (rw_country_t){ { 'C', 'M' }, 0 };
    metrics_usage_t u = { .ts = 1764100000, .on_s = 3600, .conn_s = 3500, .hand_s = 120, .hand_n = 4 };
    metrics_extra_t x = X_BASE;
    char out[METRICS_PAYLOAD_MAX + 1];
    int n = metrics_payload_build(out, sizeof(out), &u, &r, &x);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"hearts\":23,"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"level\":1,")); /* Calorcito */
    TEST_ASSERT_NOT_NULL(strstr(out, "\"seals_n\":2,"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"countries\":\"TG,CM\",\"countries_n\":2,\"stars\":1,"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"named_incunest\":0,"));
}

TEST_CASE("metrics: acompanando", "[metrics]") {
    rw_state_t r; rw_init(&r);
    metrics_usage_t u = { .ts = 1764100000 };
    metrics_extra_t x = X_BASE;
    char out[METRICS_PAYLOAD_MAX + 1];
    TEST_ASSERT_GREATER_THAN(0, metrics_payload_build(out, sizeof(out), &u, &r, &x));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"gen\":1"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"inc\":\"353\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"phase\":\"with\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"q\":3"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"fw\":\"1.2.3\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"vol\":2"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"bright\":80"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"boot_n\":5"));
}

TEST_CASE("metrics: buscando", "[metrics]") {
    rw_state_t r; rw_init(&r);
    metrics_usage_t u = { 0 };
    metrics_extra_t x = X_BASE;
    x.inc = ""; x.phase = "searching";
    char out[METRICS_PAYLOAD_MAX + 1];
    TEST_ASSERT_GREATER_THAN(0, metrics_payload_build(out, sizeof(out), &u, &r, &x));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"inc\":\"\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"phase\":\"searching\""));
}

TEST_CASE("metrics: inc y fw no rompen el JSON", "[metrics]") {
    rw_state_t r; rw_init(&r);
    metrics_usage_t u = { 0 };
    metrics_extra_t x = X_BASE;
    x.inc = "a\"b"; x.fw = "1\x02"; /* caracter de control: no es [A-Za-z0-9._-] */
    char out[METRICS_PAYLOAD_MAX + 1];
    TEST_ASSERT_GREATER_THAN(0, metrics_payload_build(out, sizeof(out), &u, &r, &x));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"inc\":\"\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"fw\":\"\""));
}

TEST_CASE("metrics: gorros, manta y Navidad", "[metrics]") {
    rw_state_t r; rw_init(&r);
    r.hat_boy = RW_ITEM_HAT_BOY_MINT; r.santa_on = 1;
    r.unlocked |= RW_BIT(RW_ITEM_SNOW);
    metrics_usage_t u = { .ts = 1764100000 };
    metrics_extra_t x = X_BASE;
    char out[METRICS_PAYLOAD_MAX + 1];
    TEST_ASSERT_GREATER_THAN(0, metrics_payload_build(out, sizeof(out), &u, &r, &x));
    char want[128];
    snprintf(want, sizeof(want), "\"hat_boy\":%u,\"hat_girl\":%u,\"blanket\":%u,\"santa\":1,\"snow\":0,",
             RW_ITEM_HAT_BOY_MINT, RW_ITEM_HAT_GIRL_BASIC, RW_ITEM_BLANKET_BASIC);
    TEST_ASSERT_NOT_NULL(strstr(out, want));
    snprintf(want, sizeof(want), "\"unlocked\":%llu,", (unsigned long long)r.unlocked);
    TEST_ASSERT_NOT_NULL(strstr(out, want));
}

TEST_CASE("metrics: pasaporte lleno cabe", "[metrics]") {
    rw_state_t r; rw_init(&r);
    r.n_countries = RW_PASSPORT_MAX;
    for (int i = 0; i < RW_PASSPORT_MAX; i++) r.passport[i] = (rw_country_t){ { 'A', (char)('A' + i % 26) }, 1 };
    r.balance = 65535; r.earned = 4000000000u;
    r.streak = r.total_days = r.babies = 65535; r.saved = r.seals = 255;
    r.unlocked = UINT64_MAX; /* 20 cifras */
    r.halo = r.bg = r.garland_on = r.hat_boy = r.hat_girl = r.blanket = r.santa_on = r.snow_on = 255;
    metrics_usage_t u = { .ts = -9223372036854775807LL - 1, .on_s = UINT32_MAX, .conn_s = UINT32_MAX,
                          .hand_s = UINT32_MAX, .hand_n = UINT32_MAX };
    /* fw e inc al tope de lo que deja pasar safe_str (47 y 23); la fase mas larga */
    metrics_extra_t x = { .gen = UINT32_MAX, .boot_n = UINT32_MAX, .demo_n = UINT32_MAX, .q = UINT32_MAX,
                          .lost = UINT32_MAX, .fw = "12345678901234567890123456789012345678901234567",
                          .inc = "12345678901234567890123", .phase = "searching", .lang = 255, .vol = 255,
                          .bright = 255 };
    char out[METRICS_PAYLOAD_MAX + 1];
    int n = metrics_payload_build(out, sizeof(out), &u, &r, &x);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"fw\":\"12345678901234567890123456789012345678901234567\""));
    TEST_ASSERT_LESS_THAN(METRICS_PAYLOAD_MAX, n); /* medido: 749 */
}

TEST_CASE("metrics: canje", "[metrics]") {
    char out[REDEEM_PAYLOAD_MAX + 1];
    TEST_ASSERT_GREATER_THAN(0, redeem_payload_build(out, sizeof(out), 1764100000));
    TEST_ASSERT_EQUAL_STRING("{\"redeem\":\"named_incunest\",\"hearts\":365,\"ts\":1764100000}", out);
}
