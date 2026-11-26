/* Corazones, racha larga, niveles, sellos, pasaporte y personalizar (rewards_logic, spec rewards). */
#include <string.h>

#include "rewards_logic.h"
#include "unity.h"

#define D0 20000 /* un dia cualquiera (dias desde epoch) */

static rw_gesture_t G(int32_t day, const char *cc, uint32_t stay)
{
    return (rw_gesture_t){ .time_valid = true, .day = day, .year = 2026, .month = 11, .mday = 10,
                           .rest_mask = 0, .country = cc, .stay = stay };
}

TEST_CASE("rewards: primer gesto del dia suma 1, el segundo nada", "[rewards]") {
    rw_state_t s; rw_init(&s);
    rw_gesture_t g = G(D0, "TG", 7);
    TEST_ASSERT_EQUAL_UINT8(1, rw_on_gesture(&s, &g).hearts);
    TEST_ASSERT_EQUAL_UINT8(0, rw_on_gesture(&s, &g).hearts);
    TEST_ASSERT_EQUAL_UINT16(1, s.balance);
    TEST_ASSERT_TRUE(s.seals & (1u << RW_SEAL_FIRST));
}

TEST_CASE("rewards: sin hora no suma", "[rewards]") {
    rw_state_t s; rw_init(&s);
    rw_gesture_t g = G(D0, "TG", 7); g.time_valid = false;
    TEST_ASSERT_EQUAL_UINT8(0, rw_on_gesture(&s, &g).hearts);
    TEST_ASSERT_EQUAL_UINT8(0, rw_on_super(&s, false, D0).hearts);
    TEST_ASSERT_EQUAL_UINT16(0, s.balance);
}

TEST_CASE("rewards: super corazon +2 una vez al dia; tope 4", "[rewards]") {
    rw_state_t s; rw_init(&s);
    for (int32_t d = 0; d < 6; d++) { rw_gesture_t g = G(D0 + d, "TG", 7); rw_on_gesture(&s, &g); }
    rw_gesture_t g = G(D0 + 6, "TG", 7); /* 7.º dia seguido: +1 +1 */
    TEST_ASSERT_EQUAL_UINT8(2, rw_on_gesture(&s, &g).hearts);
    rw_news_t n = rw_on_super(&s, true, D0 + 6);
    TEST_ASSERT_EQUAL_UINT8(2, n.hearts);
    TEST_ASSERT_TRUE(n.new_seals & (1u << RW_SEAL_SUPER));
    TEST_ASSERT_EQUAL_UINT8(0, rw_on_super(&s, true, D0 + 6).hearts);
    TEST_ASSERT_EQUAL_UINT16(6 + 4, s.balance);
    TEST_ASSERT_EQUAL_UINT8(1, s.saved);
    TEST_ASSERT_TRUE(s.seals & (1u << RW_SEAL_WEEK));
}

TEST_CASE("rewards: abrazo guardado cubre un dia sin acompañar", "[rewards]") {
    rw_state_t s; rw_init(&s);
    s.streak = 9; s.saved = 1; s.last_day = D0 - 2; /* anteayer: ayer sin gesto */
    rw_gesture_t g = G(D0, "TG", 7);
    rw_on_gesture(&s, &g);
    TEST_ASSERT_EQUAL_UINT16(10, s.streak);
    TEST_ASSERT_EQUAL_UINT8(0, s.saved);
}

TEST_CASE("rewards: sin abrazo guardado la racha vuelve a 1", "[rewards]") {
    rw_state_t s; rw_init(&s);
    s.streak = 5; s.saved = 0; s.last_day = D0 - 2;
    rw_gesture_t g = G(D0, "TG", 7);
    rw_on_gesture(&s, &g);
    TEST_ASSERT_EQUAL_UINT16(1, s.streak);
}

TEST_CASE("rewards: un dia sin bebes no rompe ni gasta", "[rewards]") {
    rw_state_t s; rw_init(&s);
    s.streak = 4; s.saved = 1; s.last_day = D0 - 2;
    rw_gesture_t g = G(D0, "TG", 7); g.rest_mask = 1u << 1; /* ayer, descanso */
    rw_on_gesture(&s, &g);
    TEST_ASSERT_EQUAL_UINT16(5, s.streak);
    TEST_ASSERT_EQUAL_UINT8(1, s.saved);
}

TEST_CASE("rewards: niveles por lo ganado; gastar no baja", "[rewards]") {
    TEST_ASSERT_EQUAL(RW_LEVEL_SPARK, rw_level(24));
    TEST_ASSERT_EQUAL(RW_LEVEL_WARMTH, rw_level(25));
    TEST_ASSERT_EQUAL(RW_LEVEL_SUN, rw_level(300));
    rw_state_t s; rw_init(&s);
    s.earned = 24; s.balance = 24; s.last_day = D0 - 1; s.streak = 1;
    rw_gesture_t g = G(D0, "TG", 7);
    TEST_ASSERT_TRUE(rw_on_gesture(&s, &g).level_up);
    uint16_t missing = 0;
    TEST_ASSERT_TRUE(rw_unlock(&s, RW_ITEM_BG_PINK, &missing)); /* 20 */
    TEST_ASSERT_EQUAL(RW_LEVEL_WARMTH, rw_level(s.earned));
}

TEST_CASE("rewards: tres bebes distintos y «a casa» con estrella", "[rewards]") {
    rw_state_t s; rw_init(&s);
    rw_gesture_t g = G(D0, "TG", 101); rw_on_gesture(&s, &g);
    g = G(D0 + 1, "TG", 101); rw_on_gesture(&s, &g); /* el mismo bebe */
    g = G(D0 + 2, "CM", 202); rw_on_gesture(&s, &g);
    TEST_ASSERT_EQUAL_UINT16(2, s.babies);
    g = G(D0 + 3, "CM", 303);
    TEST_ASSERT_TRUE(rw_on_gesture(&s, &g).new_seals & (1u << RW_SEAL_3_BABIES));
    TEST_ASSERT_EQUAL_UINT8(2, s.n_countries); /* TG, CM */
    TEST_ASSERT_EQUAL_MEMORY("TG", s.passport[0].iso, 2);
    TEST_ASSERT_EQUAL_MEMORY("CM", s.passport[1].iso, 2);
    TEST_ASSERT_EQUAL_UINT8(0, rw_on_home(&s, 999).new_seals); /* otro bebe: nada */
    TEST_ASSERT_TRUE(rw_on_home(&s, 303).new_seals & (1u << RW_SEAL_HOME));
    TEST_ASSERT_EQUAL_UINT8(1, s.passport[1].star);
    TEST_ASSERT_EQUAL_UINT8(0, s.passport[0].star);
    TEST_ASSERT_EQUAL_UINT8(0, rw_on_home(&s, 303).hearts); /* una sola vez */
}

TEST_CASE("rewards: desbloquear y sin saldo", "[rewards]") {
    rw_state_t s; rw_init(&s); s.balance = 18;
    uint16_t missing = 0;
    TEST_ASSERT_TRUE(rw_unlock(&s, RW_ITEM_HALO_LAVENDER, &missing));
    TEST_ASSERT_EQUAL_UINT16(3, s.balance);
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_HALO_LAVENDER, s.halo);
    TEST_ASSERT_FALSE(rw_unlock(&s, RW_ITEM_HALO_PEACH, &missing));
    TEST_ASSERT_EQUAL_UINT16(17, missing);
    TEST_ASSERT_TRUE(rw_unlock(&s, RW_ITEM_HALO_SKY, &missing)); /* de serie: elegir gratis */
    TEST_ASSERT_TRUE(rw_unlock(&s, RW_ITEM_HALO_LAVENDER, &missing)); /* ya suyo: gratis */
    TEST_ASSERT_EQUAL_UINT16(3, s.balance);
    TEST_ASSERT_FALSE(rw_unlock(&s, RW_ITEM_GARLAND, &missing)); /* no se compra */
}

TEST_CASE("rewards: una IncuNest con tu nombre se canjea una vez", "[rewards]") {
    rw_state_t s; rw_init(&s); s.balance = 400;
    uint16_t missing = 0;
    TEST_ASSERT_TRUE(rw_unlock(&s, RW_ITEM_NAMED_INCUNEST, &missing));
    TEST_ASSERT_EQUAL_UINT16(35, s.balance);
    TEST_ASSERT_FALSE(rw_unlock(&s, RW_ITEM_NAMED_INCUNEST, &missing)); /* ya pedida */
    TEST_ASSERT_EQUAL_UINT16(35, s.balance);
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_HALO_SKY, s.halo); /* no cambia lo elegido */
    rw_state_t t; rw_init(&t); t.balance = 100;
    TEST_ASSERT_FALSE(rw_unlock(&t, RW_ITEM_NAMED_INCUNEST, &missing));
    TEST_ASSERT_EQUAL_UINT16(265, missing);
}

TEST_CASE("rewards: guirnalda al 10.º dia de diciembre", "[rewards]") {
    rw_state_t s; rw_init(&s);
    rw_news_t n = { 0 };
    for (int d = 1; d <= 10; d++) {
        rw_gesture_t g = G(rw_day_of(2026, 12, d), "TG", 7);
        g.month = 12; g.mday = d; g.year = 2026;
        n = rw_on_gesture(&s, &g);
        if (d < 10) TEST_ASSERT_FALSE(n.garland);
    }
    TEST_ASSERT_TRUE(n.garland);
    TEST_ASSERT_TRUE(s.unlocked & (1u << RW_ITEM_GARLAND));
    TEST_ASSERT_TRUE(rw_garland_season(1, 6));
    TEST_ASSERT_FALSE(rw_garland_season(1, 7));
}

TEST_CASE("rewards: semilla de la racha desde accompany", "[rewards]") {
    rw_state_t s; rw_init(&s);
    /* ayer y anteayer con gesto, el anterior descanso, el de antes con gesto; hoy aun no */
    rw_seed_streak(&s, (1u << 1) | (1u << 2) | (1u << 4), 1u << 3, D0);
    TEST_ASSERT_EQUAL_UINT16(3, s.streak);
    TEST_ASSERT_EQUAL_INT32(D0 - 1, s.last_day);
    TEST_ASSERT_EQUAL_UINT16(0, s.balance); /* la racha se respeta, no da corazones */
}

TEST_CASE("rewards: demo aparte", "[rewards]") {
    rw_state_t real; rw_init(&real); real.balance = 40;
    rw_state_t demo; rw_demo_seed(&demo);
    TEST_ASSERT_EQUAL_UINT16(12, demo.balance);
    rw_gesture_t g = G(D0, "CM", 1);
    rw_on_gesture(&demo, &g);
    TEST_ASSERT_EQUAL_UINT16(13, demo.balance);
    TEST_ASSERT_EQUAL_UINT16(40, real.balance);
}

TEST_CASE("rewards: dias civiles seguidos", "[rewards]") {
    TEST_ASSERT_EQUAL_INT32(0, rw_day_of(1970, 1, 1));
    TEST_ASSERT_EQUAL_INT32(rw_day_of(2026, 2, 28) + 1, rw_day_of(2026, 3, 1));
    TEST_ASSERT_EQUAL_INT32(rw_day_of(2026, 12, 31) + 1, rw_day_of(2027, 1, 1));
}

TEST_CASE("rewards: corazones de la IncuPray fuera del tope y apuesta", "[rewards]") {
    rw_state_t s; rw_init(&s); s.earned = 20; s.balance = 5;
    rw_news_t n = rw_add(&s, 10);
    TEST_ASSERT_EQUAL_UINT8(10, n.hearts);
    TEST_ASSERT_TRUE(n.level_up); /* 30 ganados: Calorcito */
    TEST_ASSERT_EQUAL_UINT16(15, s.balance);
    TEST_ASSERT_TRUE(rw_spend(&s, 3));
    TEST_ASSERT_EQUAL_UINT16(12, s.balance);
    TEST_ASSERT_FALSE(rw_spend(&s, 20));
    TEST_ASSERT_EQUAL_UINT16(12, s.balance);
    TEST_ASSERT_EQUAL_UINT32(30, s.earned); /* gastar no baja lo ganado */
}
