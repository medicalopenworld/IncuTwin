/* Anillo del historial de avisos (event_ring): orden, desborde y "visto". */
#include "event_ring.h"
#include "unity.h"

static const event_entry_t *E(uint32_t at, uint8_t kind)
{
    static event_entry_t s;
    s = (event_entry_t){ .at = at, .kind = kind, .inc = "SIM01" };
    return &s;
}

TEST_CASE("ring: el mas reciente primero", "[evlog]") {
    event_ring_t r = {0};
    event_ring_push(&r, E(100, TWIN_NOTICE_ADMISSION)); event_ring_push(&r, E(200, TWIN_NOTICE_HEAT_ON));
    event_entry_t o; TEST_ASSERT_TRUE(event_ring_get(&r, 0, &o));
    TEST_ASSERT_EQUAL_UINT32(200, o.at); TEST_ASSERT_EQUAL_UINT8(2, r.unseen);
}

TEST_CASE("ring: 51 entradas conservan las 50 mas nuevas", "[evlog]") {
    event_ring_t r = {0};
    for (uint32_t i = 1; i <= 51; i++) event_ring_push(&r, E(i, TWIN_NOTICE_EXIT));
    event_entry_t o;
    TEST_ASSERT_EQUAL_UINT8(50, r.count); TEST_ASSERT_EQUAL_UINT8(50, r.unseen);
    event_ring_get(&r, 0, &o); TEST_ASSERT_EQUAL_UINT32(51, o.at);
    event_ring_get(&r, 49, &o); TEST_ASSERT_EQUAL_UINT32(2, o.at);
    TEST_ASSERT_FALSE(event_ring_get(&r, 50, &o));
}

TEST_CASE("ring: marcar visto", "[evlog]") {
    event_ring_t r = {0}; event_ring_push(&r, E(1, TWIN_NOTICE_EXIT));
    event_ring_mark_seen(&r); TEST_ASSERT_EQUAL_UINT8(0, r.unseen);
}
