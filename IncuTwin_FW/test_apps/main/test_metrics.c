/* Payloads de metricas y canje (metrics_payload, spec device-metrics). */
#include <stdio.h>
#include <string.h>

#include "metrics_payload.h"
#include "unity.h"

TEST_CASE("metrics: uso, corazones y pasaporte", "[metrics]") {
    rw_state_t r; rw_init(&r);
    r.balance = 23; r.earned = 30; r.streak = 9; r.saved = 1; r.total_days = 12; r.babies = 2;
    r.seals = 0x03; r.n_countries = 2;
    r.passport[0] = (rw_country_t){ { 'T', 'G' }, 1 };
    r.passport[1] = (rw_country_t){ { 'C', 'M' }, 0 };
    metrics_usage_t u = { .ts = 1764100000, .on_s = 3600, .conn_s = 3500, .hand_s = 120, .hand_n = 4 };
    char out[METRICS_PAYLOAD_MAX + 1];
    int n = metrics_payload_build(out, sizeof(out), &u, &r);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"hearts\":23,"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"level\":1,")); /* Calorcito */
    TEST_ASSERT_NOT_NULL(strstr(out, "\"seals_n\":2,"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"countries\":\"TG,CM\",\"countries_n\":2,\"stars\":1,"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"named_incunest\":0,"));
}

TEST_CASE("metrics: gorros, manta y Navidad", "[metrics]") {
    rw_state_t r; rw_init(&r);
    r.hat_boy = RW_ITEM_HAT_BOY_MINT; r.santa_on = 1;
    r.unlocked |= RW_BIT(RW_ITEM_SNOW);
    metrics_usage_t u = { .ts = 1764100000 };
    char out[METRICS_PAYLOAD_MAX + 1];
    TEST_ASSERT_GREATER_THAN(0, metrics_payload_build(out, sizeof(out), &u, &r));
    char want[128];
    snprintf(want, sizeof(want), "\"hat_boy\":%u,\"hat_girl\":%u,\"blanket\":%u,\"santa\":1,\"snow\":0}",
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
    metrics_usage_t u = { .ts = 9999999999LL, .on_s = 4000000000u, .conn_s = 4000000000u,
                          .hand_s = 4000000000u, .hand_n = 4000000000u };
    char out[METRICS_PAYLOAD_MAX + 1];
    TEST_ASSERT_GREATER_THAN(0, metrics_payload_build(out, sizeof(out), &u, &r));
}

TEST_CASE("metrics: canje", "[metrics]") {
    char out[REDEEM_PAYLOAD_MAX + 1];
    TEST_ASSERT_GREATER_THAN(0, redeem_payload_build(out, sizeof(out), 1764100000));
    TEST_ASSERT_EQUAL_STRING("{\"redeem\":\"named_incunest\",\"hearts\":365,\"ts\":1764100000}", out);
}
