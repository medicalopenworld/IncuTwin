/* Specs broker-link (backoff) y device-status (payload). */
#include <string.h>

#include "backoff.h"
#include "status_payload.h"
#include "unity.h"

TEST_CASE("backoff: 1,2,4,8,16,32,60,60 y reinicio", "[mqtt]")
{
    backoff_t b;
    backoff_init(&b, 1, 60);
    const uint32_t expected[] = { 1, 2, 4, 8, 16, 32, 60, 60 };
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_UINT32(expected[i], backoff_next_nominal(&b));
    }
    backoff_reset(&b);
    TEST_ASSERT_EQUAL_UINT32(1, backoff_next_nominal(&b));
}

TEST_CASE("backoff: jitter dentro de +-20 %", "[mqtt]")
{
    TEST_ASSERT_EQUAL_UINT32(8000, backoff_apply_jitter_ms(10, 0));            /* -20 % */
    TEST_ASSERT_EQUAL_UINT32(12000, backoff_apply_jitter_ms(10, 0xFFFFFFFFu)); /* +20 % */
    uint32_t mid = backoff_apply_jitter_ms(10, 0x80000000u);
    TEST_ASSERT_TRUE(mid >= 9900 && mid <= 10100);
    TEST_ASSERT_EQUAL_UINT32(800, backoff_apply_jitter_ms(1, 0));
    TEST_ASSERT_EQUAL_UINT32(1200, backoff_apply_jitter_ms(1, 0xFFFFFFFFu));
}

TEST_CASE("status: payload exacto y tamano", "[status]")
{
    char buf[STATUS_PAYLOAD_MAX + 1];
    int n = status_payload_build(buf, sizeof(buf), "2.0.0", -61, "353", 1759320000LL);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_STRING(
        "{\"online\":true,\"fw\":\"2.0.0\",\"rssi\":-61,\"incubator_id\":\"353\",\"ts\":1759320000}", buf);
    TEST_ASSERT_TRUE(n <= STATUS_PAYLOAD_MAX);

    n = status_payload_build(buf, sizeof(buf), "2.0.0", -100, "", 0);
    TEST_ASSERT_EQUAL_STRING("{\"online\":true,\"fw\":\"2.0.0\",\"rssi\":-100,\"incubator_id\":\"\",\"ts\":0}", buf);

    /* el peor caso cabe: version larga + id de 16 + ts de 10 digitos */
    n = status_payload_build(buf, sizeof(buf), "12.34.56-beta.7", -99, "1234567890123456", 9999999999LL);
    TEST_ASSERT_TRUE(n > 0 && n <= STATUS_PAYLOAD_MAX);
    char tiny[20];
    TEST_ASSERT_EQUAL(-1, status_payload_build(tiny, sizeof(tiny), "2.0.0", -61, "353", 0));
}
