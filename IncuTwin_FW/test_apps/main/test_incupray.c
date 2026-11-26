/* IncuPray (incupray_logic, spec incupray): ventana, avisos, apuesta, corazones y mensajes. */
#include <string.h>

#include "incupray_logic.h"
#include "unity.h"

#define T0 1765047600u /* un domingo a las 20:00 */

static ip_window_t W(void)
{
    ip_window_t w = { .start = T0, .duration_s = 600 };
    strcpy(w.id, "2026-12-06");
    return w;
}

TEST_CASE("incupray: ventana y lo que queda", "[incupray]") {
    ip_window_t w = W();
    TEST_ASSERT_EQUAL(IP_BEFORE, ip_phase(&w, T0 - 1));
    TEST_ASSERT_EQUAL(IP_OPEN, ip_phase(&w, T0 + 240));
    TEST_ASSERT_EQUAL_UINT32(360, ip_remaining_s(&w, T0 + 240));
    TEST_ASSERT_EQUAL(IP_AFTER, ip_phase(&w, T0 + 600));
    TEST_ASSERT_EQUAL_UINT32(0, ip_remaining_s(&w, T0 + 700));
    TEST_ASSERT_EQUAL(IP_BEFORE, ip_phase(&w, 0)); /* sin hora: no hay IncuPray */
}

TEST_CASE("incupray: avisos por tramos, una vez cada uno", "[incupray]") {
    ip_window_t w = W();
    TEST_ASSERT_EQUAL(IP_NOTICE_NONE, ip_notice_due(&w, T0 - IP_EVE_S - 1, 0));
    TEST_ASSERT_EQUAL(IP_NOTICE_EVE, ip_notice_due(&w, T0 - IP_EVE_S, 0));
    TEST_ASSERT_EQUAL(IP_NOTICE_NONE, ip_notice_due(&w, T0 - 7200, IP_NOTICE_EVE));
    TEST_ASSERT_EQUAL(IP_NOTICE_HOUR, ip_notice_due(&w, T0 - 3600, IP_NOTICE_EVE));
    TEST_ASSERT_EQUAL(IP_NOTICE_START, ip_notice_due(&w, T0 + 5, IP_NOTICE_EVE | IP_NOTICE_HOUR));
    TEST_ASSERT_EQUAL(IP_NOTICE_START, ip_notice_due(&w, T0 + 5, 0)); /* encendida tarde: solo la de inicio */
    TEST_ASSERT_EQUAL(IP_NOTICE_NONE, ip_notice_due(&w, T0 + 600, 0));
}

TEST_CASE("incupray: apuesta y corazones por minuto", "[incupray]") {
    TEST_ASSERT_EQUAL_UINT16(3, ip_stake(20));
    TEST_ASSERT_EQUAL_UINT16(0, ip_stake(2)); /* gratis */
    TEST_ASSERT_EQUAL_UINT8(0, ip_hearts_for(59));
    TEST_ASSERT_EQUAL_UINT8(2, ip_hearts_for(150));
    TEST_ASSERT_EQUAL_UINT8(10, ip_hearts_for(3600));
}

TEST_CASE("incupray: mensajes del broker", "[incupray]") {
    const char *next = "{\"id\":\"2026-12-06\",\"start\":1765047600,\"duration_s\":600}";
    ip_window_t w;
    TEST_ASSERT_TRUE(ip_parse_next(next, strlen(next), &w));
    TEST_ASSERT_EQUAL_STRING("2026-12-06", w.id);
    TEST_ASSERT_EQUAL_UINT32(600, w.duration_s);
    TEST_ASSERT_FALSE(ip_parse_next("{\"id\":\"x\"}", 10, &w));

    static ip_live_t l;
    const char *live = "{\"id\":\"2026-12-06\",\"count\":214,\"hearts\":1380,"
                       "\"names\":[\"Lucía y Pablo\",\"ITW-0A1284\"],"
                       "\"babies\":[{\"inc\":\"297\",\"country\":\"TG\",\"sex\":\"f\",\"skin\":3},"
                       "{\"inc\":\"353\",\"country\":\"CM\",\"sex\":\"\",\"skin\":2}]}";
    TEST_ASSERT_TRUE(ip_parse_live(live, strlen(live), &l));
    TEST_ASSERT_EQUAL_UINT16(214, l.count);
    TEST_ASSERT_EQUAL_UINT8(2, l.n_names);
    TEST_ASSERT_EQUAL_STRING("Lucía y Pablo", l.names[0]);
    TEST_ASSERT_EQUAL_UINT8(2, l.n_babies);
    TEST_ASSERT_EQUAL_UINT8(TWIN_SEX_FEMALE, l.babies[0].sex);
    TEST_ASSERT_EQUAL_UINT8(TWIN_SEX_UNKNOWN, l.babies[1].sex);
    TEST_ASSERT_EQUAL_STRING("CM", l.babies[1].country);

    ip_summary_t s;
    const char *sum = "{\"id\":\"2026-12-06\",\"count\":214,\"hearts\":1380}";
    TEST_ASSERT_TRUE(ip_parse_summary(sum, strlen(sum), &s));
    TEST_ASSERT_EQUAL_UINT32(1380, s.hearts);

    char out[IP_PARTICIPATION_MAX + 1];
    TEST_ASSERT_GREATER_THAN(0, ip_participation_build(out, sizeof(out), "2026-12-06", 240, 4));
    TEST_ASSERT_EQUAL_STRING("{\"id\":\"2026-12-06\",\"joined\":true,\"held_s\":240,\"hearts\":4}", out);
}
