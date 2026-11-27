/* Corazones, racha larga, niveles, sellos, pasaporte y personalizar (rewards_logic, spec rewards). */
#include <stddef.h>
#include <string.h>

#include "rewards_logic.h"
#include "unity.h"

#define D0 20000 /* un dia cualquiera (dias desde epoch) */
#define NOV 11, 10 /* mes, dia: fuera de Navidad */
#define XMAS 12, 25

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

TEST_CASE("rewards: niveles de afecto por lo ganado; gastar no baja", "[rewards]") {
    TEST_ASSERT_EQUAL(RW_LEVEL_HAND, rw_level(9));
    TEST_ASSERT_EQUAL(RW_LEVEL_CARESS, rw_level(10));
    TEST_ASSERT_EQUAL(RW_LEVEL_TICKLES, rw_level(30));
    TEST_ASSERT_EQUAL(RW_LEVEL_HUG, rw_level(75));
    TEST_ASSERT_EQUAL(RW_LEVEL_SQUEEZE, rw_level(150));
    TEST_ASSERT_EQUAL_UINT32(10, rw_level_min(RW_LEVEL_CARESS));
    rw_state_t s; rw_init(&s);
    s.earned = 9; s.balance = 9; s.last_day = D0 - 1; s.streak = 1;
    rw_gesture_t g = G(D0, "TG", 7);
    TEST_ASSERT_TRUE(rw_on_gesture(&s, &g).level_up);
    uint16_t missing = 0;
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_BG_CREAM, NOV, &missing)); /* 10 */
    TEST_ASSERT_EQUAL(RW_LEVEL_CARESS, rw_level(s.earned));
}

TEST_CASE("rewards: quien ya tenia corazones recibe el aviso una vez", "[rewards]") {
    rw_state_t s; rw_init(&s);
    TEST_ASSERT_FALSE(rw_catchup_due(&s));          /* Manita: nada que anunciar */
    s.earned = 40;                                   /* guardado antiguo: level_seen = 0 */
    TEST_ASSERT_TRUE(rw_catchup_due(&s));
    rw_catchup_done(&s);
    TEST_ASSERT_FALSE(rw_catchup_due(&s));
    TEST_ASSERT_EQUAL_UINT8(RW_LEVEL_TICKLES, s.level_seen);
    s.level_seen = RW_LEVEL_SQUEEZE;                 /* por delante del nivel calculado: nada que anunciar */
    TEST_ASSERT_FALSE(rw_catchup_due(&s));
}

TEST_CASE("rewards: subir de nivel jugando no deja aviso pendiente", "[rewards]") {
    rw_state_t s; rw_init(&s);
    s.earned = 9; s.last_day = D0 - 1; s.streak = 1;
    rw_gesture_t g = G(D0, "TG", 7);
    TEST_ASSERT_TRUE(rw_on_gesture(&s, &g).level_up);
    TEST_ASSERT_FALSE(rw_catchup_due(&s));          /* ya lo anuncia el aviso normal */
}

TEST_CASE("rewards: el aviso no tapa los niveles que aun no abren nada", "[rewards]") {
    rw_state_t s; rw_init(&s);
    s.earned = 80;                                   /* Abrazo, que en esta version no abre nada */
    TEST_ASSERT_TRUE(rw_catchup_due(&s));            /* aun asi, Caricia y Cosquillitas estan por anunciar */
    rw_catchup_done(&s);
    TEST_ASSERT_EQUAL_UINT8(RW_LEVEL_LAST_OPEN, s.level_seen); /* no se guarda el nivel real */
    TEST_ASSERT_EQUAL_UINT8(RW_LEVEL_TICKLES, s.level_seen);
    TEST_ASSERT_FALSE(rw_catchup_due(&s));
}

TEST_CASE("rewards: subir a Abrazo jugando deja anunciado solo hasta Cosquillitas", "[rewards]") {
    rw_state_t s; rw_init(&s);
    s.earned = 74; s.last_day = D0 - 1; s.streak = 1;
    rw_gesture_t g = G(D0, "TG", 7);
    TEST_ASSERT_TRUE(rw_on_gesture(&s, &g).level_up);
    TEST_ASSERT_EQUAL_UINT(RW_LEVEL_HUG, rw_level(s.earned));
    TEST_ASSERT_EQUAL_UINT8(RW_LEVEL_TICKLES, s.level_seen);
    TEST_ASSERT_FALSE(rw_catchup_due(&s));
}

TEST_CASE("rewards: el guardado no cambia de tamaño", "[rewards]") {
    TEST_ASSERT_EQUAL_UINT(offsetof(rw_state_t, passport) - 19, offsetof(rw_state_t, level_seen));
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
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_HALO_LAVENDER, NOV, &missing));
    TEST_ASSERT_EQUAL_UINT16(3, s.balance);
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_HALO_LAVENDER, s.halo);
    TEST_ASSERT_EQUAL(RW_UNLOCK_NO_FUNDS, rw_unlock(&s, RW_ITEM_HALO_PEACH, NOV, &missing));
    TEST_ASSERT_EQUAL_UINT16(17, missing);
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_HALO_SKY, NOV, &missing)); /* de serie: elegir gratis */
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_HALO_LAVENDER, NOV, &missing)); /* ya suyo: gratis */
    TEST_ASSERT_EQUAL_UINT16(3, s.balance);
    TEST_ASSERT_EQUAL(RW_UNLOCK_NOT_FOR_SALE, rw_unlock(&s, RW_ITEM_GARLAND, XMAS, &missing)); /* no se compra */
}

TEST_CASE("rewards: una IncuNest con tu nombre se canjea una vez", "[rewards]") {
    rw_state_t s; rw_init(&s); s.balance = 400;
    uint16_t missing = 0;
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_NAMED_INCUNEST, NOV, &missing));
    TEST_ASSERT_EQUAL_UINT16(35, s.balance);
    TEST_ASSERT_EQUAL(RW_UNLOCK_DONE, rw_unlock(&s, RW_ITEM_NAMED_INCUNEST, NOV, &missing)); /* ya pedida */
    TEST_ASSERT_EQUAL_UINT16(35, s.balance);
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_HALO_SKY, s.halo); /* no cambia lo elegido */
    rw_state_t t; rw_init(&t); t.balance = 100;
    TEST_ASSERT_EQUAL(RW_UNLOCK_NO_FUNDS, rw_unlock(&t, RW_ITEM_NAMED_INCUNEST, NOV, &missing));
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
    /* se gana en diciembre, pero solo se ve del 24-dic al 6-ene */
    TEST_ASSERT_FALSE(rw_wears(&s, RW_ITEM_GARLAND, 12, 10));
    TEST_ASSERT_TRUE(rw_wears(&s, RW_ITEM_GARLAND, 12, 24));
    TEST_ASSERT_TRUE(rw_wears(&s, RW_ITEM_GARLAND, 1, 6));
    TEST_ASSERT_FALSE(rw_wears(&s, RW_ITEM_GARLAND, 1, 7));
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
    TEST_ASSERT_EQUAL_UINT16(50, demo.balance);
    TEST_ASSERT_FALSE(rw_catchup_due(&demo)); /* la demo no nace con aviso pendiente */
    rw_gesture_t g = G(D0, "CM", 1);
    rw_on_gesture(&demo, &g);
    TEST_ASSERT_EQUAL_UINT16(51, demo.balance);
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
    TEST_ASSERT_TRUE(n.level_up); /* 30 ganados: Cosquillitas */
    TEST_ASSERT_EQUAL_UINT16(15, s.balance);
    TEST_ASSERT_TRUE(rw_spend(&s, 3));
    TEST_ASSERT_EQUAL_UINT16(12, s.balance);
    TEST_ASSERT_FALSE(rw_spend(&s, 20));
    TEST_ASSERT_EQUAL_UINT16(12, s.balance);
    TEST_ASSERT_EQUAL_UINT32(30, s.earned); /* gastar no baja lo ganado */
}

/* ------------------------------------------------------------------ tienda (cambio tienda) */

TEST_CASE("tienda: de serie gorros y manta Basicos", "[rewards]") {
    rw_state_t s; rw_init(&s);
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_HAT_BOY_BASIC, rw_hat_for(&s, false, NOV));
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_HAT_GIRL_BASIC, rw_hat_for(&s, true, NOV));
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_BLANKET_BASIC, s.blanket);
    TEST_ASSERT_TRUE(s.unlocked & RW_BIT(RW_ITEM_HAT_BOY_BASIC));
    TEST_ASSERT_TRUE(s.unlocked & RW_BIT(RW_ITEM_HAT_GIRL_BASIC));
    TEST_ASSERT_TRUE(s.unlocked & RW_BIT(RW_ITEM_BLANKET_BASIC));
}

TEST_CASE("tienda: gorro de nino no cambia el de nina; manta para todos", "[rewards]") {
    rw_state_t s; rw_init(&s); s.balance = 20;
    uint16_t missing = 0;
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_HAT_BOY_MINT, NOV, &missing));
    TEST_ASSERT_EQUAL_UINT16(5, s.balance);
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_HAT_BOY_MINT, rw_hat_for(&s, false, NOV));
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_HAT_GIRL_BASIC, rw_hat_for(&s, true, NOV));
    TEST_ASSERT_EQUAL(RW_UNLOCK_NO_FUNDS, rw_unlock(&s, RW_ITEM_BLANKET_CREAM, NOV, &missing));
    TEST_ASSERT_EQUAL_UINT16(10, missing);
    s.balance = 15;
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_BLANKET_CREAM, NOV, &missing));
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_BLANKET_CREAM, s.blanket);
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_HAT_BOY_BASIC, NOV, &missing)); /* volver: gratis */
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_HAT_BOY_BASIC, rw_hat_for(&s, false, NOV));
    TEST_ASSERT_EQUAL_UINT16(0, s.balance);
}

TEST_CASE("tienda: precios", "[rewards]") {
    TEST_ASSERT_EQUAL_UINT16(15, rw_price(RW_ITEM_HAT_BOY_MINT));
    TEST_ASSERT_EQUAL_UINT16(15, rw_price(RW_ITEM_HAT_BOY_CHICK));
    TEST_ASSERT_EQUAL_UINT16(20, rw_price(RW_ITEM_HAT_BOY_CLOUD));
    TEST_ASSERT_EQUAL_UINT16(15, rw_price(RW_ITEM_HAT_GIRL_LILAC));
    TEST_ASSERT_EQUAL_UINT16(15, rw_price(RW_ITEM_HAT_GIRL_PEACH));
    TEST_ASSERT_EQUAL_UINT16(20, rw_price(RW_ITEM_HAT_GIRL_MINT));
    TEST_ASSERT_EQUAL_UINT16(15, rw_price(RW_ITEM_BLANKET_CREAM));
    TEST_ASSERT_EQUAL_UINT16(15, rw_price(RW_ITEM_BLANKET_MINT));
    TEST_ASSERT_EQUAL_UINT16(20, rw_price(RW_ITEM_BLANKET_LAVENDER));
    TEST_ASSERT_EQUAL_UINT16(20, rw_price(RW_ITEM_BLANKET_PEACH));
    TEST_ASSERT_EQUAL_UINT16(10, rw_price(RW_ITEM_SANTA));
    TEST_ASSERT_EQUAL_UINT16(10, rw_price(RW_ITEM_SNOW));
    TEST_ASSERT_EQUAL_UINT16(0, rw_price(RW_ITEM_HAT_GIRL_BASIC));
}

TEST_CASE("tienda: Navidad solo del 24-dic al 6-ene", "[rewards]") {
    TEST_ASSERT_FALSE(rw_xmas_season(12, 23));
    TEST_ASSERT_TRUE(rw_xmas_season(12, 24));
    TEST_ASSERT_TRUE(rw_xmas_season(12, 31));
    TEST_ASSERT_TRUE(rw_xmas_season(1, 1));
    TEST_ASSERT_TRUE(rw_xmas_season(1, 6));
    TEST_ASSERT_FALSE(rw_xmas_season(1, 7));
    TEST_ASSERT_FALSE(rw_xmas_season(0, 0)); /* sin hora */
    TEST_ASSERT_TRUE(rw_is_xmas(RW_ITEM_SANTA));
    TEST_ASSERT_TRUE(rw_is_xmas(RW_ITEM_SNOW));
    TEST_ASSERT_TRUE(rw_is_xmas(RW_ITEM_GARLAND));
    TEST_ASSERT_FALSE(rw_is_xmas(RW_ITEM_HAT_BOY_MINT));

    rw_state_t s; rw_init(&s); s.balance = 30;
    uint16_t missing = 0;
    TEST_ASSERT_EQUAL(RW_UNLOCK_OFF_SEASON, rw_unlock(&s, RW_ITEM_SANTA, 12, 20, &missing));
    TEST_ASSERT_EQUAL(RW_UNLOCK_OFF_SEASON, rw_unlock(&s, RW_ITEM_SANTA, 0, 0, &missing)); /* sin hora */
    TEST_ASSERT_EQUAL_UINT16(30, s.balance);
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_SANTA, 12, 24, &missing));
    TEST_ASSERT_EQUAL_UINT16(20, s.balance);
    TEST_ASSERT_EQUAL_UINT8(1, s.santa_on);
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_SANTA, rw_hat_for(&s, false, 12, 25));
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_SANTA, rw_hat_for(&s, true, 1, 6));
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_HAT_BOY_BASIC, rw_hat_for(&s, false, 1, 7)); /* despues de Reyes */
    TEST_ASSERT_EQUAL_UINT8(1, s.santa_on); /* sigue puesto para el año que viene */
    /* fuera de temporada tampoco se pone ni se quita */
    TEST_ASSERT_EQUAL(RW_UNLOCK_OFF_SEASON, rw_unlock(&s, RW_ITEM_SANTA, 3, 1, &missing));
    TEST_ASSERT_EQUAL_UINT8(1, s.santa_on);
    /* en temporada: tocarlo lo quita; elegir otro gorro tambien */
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_SANTA, 12, 26, &missing));
    TEST_ASSERT_EQUAL_UINT8(0, s.santa_on);
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_SANTA, 12, 26, &missing));
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_HAT_BOY_BASIC, 12, 26, &missing));
    TEST_ASSERT_EQUAL_UINT8(0, s.santa_on);
    TEST_ASSERT_EQUAL_UINT16(20, s.balance);

    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_SNOW, 1, 2, &missing));
    TEST_ASSERT_TRUE(rw_wears(&s, RW_ITEM_SNOW, 1, 2));
    TEST_ASSERT_FALSE(rw_wears(&s, RW_ITEM_SNOW, 1, 8));
    TEST_ASSERT_EQUAL_UINT16(10, s.balance);
}

TEST_CASE("tienda: la guirnalda conseguida solo se pone desde el 24", "[rewards]") {
    rw_state_t s; rw_init(&s);
    s.unlocked |= RW_BIT(RW_ITEM_GARLAND); s.garland_on = 1;
    uint16_t missing = 0;
    TEST_ASSERT_EQUAL(RW_UNLOCK_OFF_SEASON, rw_unlock(&s, RW_ITEM_GARLAND, 12, 15, &missing));
    TEST_ASSERT_EQUAL_UINT8(1, s.garland_on);
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_GARLAND, 12, 24, &missing)); /* quitar */
    TEST_ASSERT_EQUAL_UINT8(0, s.garland_on);
}

TEST_CASE("tienda: todo cuesta, tambien tras Primera mano", "[rewards]") {
    /* Pablo, 26-nov: nada gratis en la tienda salvo lo de serie */
    for (int i = 0; i < RW_ITEM_COUNT; i++) {
        bool stock = i == RW_ITEM_HALO_SKY || i == RW_ITEM_BG_DEFAULT || i == RW_ITEM_HAT_BOY_BASIC ||
                     i == RW_ITEM_HAT_GIRL_BASIC || i == RW_ITEM_BLANKET_BASIC;
        if (stock || i == RW_ITEM_GARLAND) continue; /* la guirnalda no se compra: se gana */
        TEST_ASSERT_GREATER_THAN_UINT16(0, rw_price((rw_item_t)i));
    }
    rw_state_t s; rw_init(&s);
    rw_gesture_t g = G(D0, "TG", 7);
    rw_on_gesture(&s, &g); /* «Primera mano»: saldo 1 */
    uint16_t missing = 0;
    TEST_ASSERT_EQUAL(RW_UNLOCK_NO_FUNDS, rw_unlock(&s, RW_ITEM_HAT_GIRL_LILAC, NOV, &missing));
    TEST_ASSERT_EQUAL_UINT16(14, missing);
    TEST_ASSERT_EQUAL_UINT16(1, s.balance);
}

TEST_CASE("tienda: migracion desde la version 1", "[rewards]") {
    rw_state_v1_t v1; memset(&v1, 0, sizeof(v1));
    v1.version = 1; v1.saved = 1; v1.seals = (1u << RW_SEAL_FIRST) | (1u << RW_SEAL_WEEK); v1.n_countries = 2;
    v1.balance = 40; v1.streak = 9; v1.total_days = 12; v1.babies = 2;
    v1.unlocked = (1u << RW_ITEM_HALO_SKY) | (1u << RW_ITEM_BG_DEFAULT) | (1u << RW_ITEM_HALO_LAVENDER) |
                  (1u << RW_ITEM_GARLAND) | (1u << RW_ITEM_NAMED_INCUNEST);
    v1.dec_year = 2026; v1.earned = 55; v1.last_day = D0; v1.super_day = D0 - 3; v1.dec_days = 0x6;
    v1.last_stay = 77; v1.last_stay_cc[0] = 'T'; v1.last_stay_cc[1] = 'G'; v1.last_stay_home = 1;
    v1.halo = RW_ITEM_HALO_LAVENDER; v1.bg = RW_ITEM_BG_DEFAULT; v1.garland_on = 1; v1.redeem_sent = 1;
    v1.passport[0] = (rw_country_t){ { 'T', 'G' }, 1 };
    v1.passport[1] = (rw_country_t){ { 'C', 'M' }, 0 };

    rw_state_t s;
    TEST_ASSERT_TRUE(rw_migrate_v1(&s, &v1));
    TEST_ASSERT_EQUAL_UINT8(RW_VERSION, s.version);
    TEST_ASSERT_EQUAL_UINT16(40, s.balance);
    TEST_ASSERT_EQUAL_UINT32(55, s.earned);
    TEST_ASSERT_EQUAL_UINT8(0, s.level_seen); /* el aviso pendiente saldra una vez */
    TEST_ASSERT_EQUAL_UINT16(9, s.streak);
    TEST_ASSERT_EQUAL_UINT8(1, s.saved);
    TEST_ASSERT_EQUAL_UINT16(12, s.total_days);
    TEST_ASSERT_EQUAL_UINT16(2, s.babies);
    TEST_ASSERT_EQUAL_UINT8(v1.seals, s.seals);
    TEST_ASSERT_EQUAL_INT32(D0, s.last_day);
    TEST_ASSERT_EQUAL_INT32(D0 - 3, s.super_day);
    TEST_ASSERT_EQUAL_UINT16(2026, s.dec_year);
    TEST_ASSERT_EQUAL_UINT32(0x6, s.dec_days);
    TEST_ASSERT_EQUAL_UINT32(77, s.last_stay);
    TEST_ASSERT_EQUAL_MEMORY("TG", s.last_stay_cc, 2);
    TEST_ASSERT_EQUAL_UINT8(1, s.last_stay_home);
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_HALO_LAVENDER, s.halo);
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_BG_DEFAULT, s.bg);
    TEST_ASSERT_EQUAL_UINT8(1, s.garland_on);
    TEST_ASSERT_EQUAL_UINT8(1, s.redeem_sent);
    TEST_ASSERT_EQUAL_UINT8(2, s.n_countries);
    TEST_ASSERT_EQUAL_MEMORY("CM", s.passport[1].iso, 2);
    TEST_ASSERT_EQUAL_UINT8(1, s.passport[0].star);
    TEST_ASSERT_TRUE(s.unlocked & RW_BIT(RW_ITEM_HALO_LAVENDER));
    TEST_ASSERT_TRUE(s.unlocked & RW_BIT(RW_ITEM_GARLAND));
    TEST_ASSERT_TRUE(s.unlocked & RW_BIT(RW_ITEM_NAMED_INCUNEST));
    TEST_ASSERT_TRUE(s.unlocked & RW_BIT(RW_ITEM_HAT_BOY_BASIC));
    TEST_ASSERT_TRUE(s.unlocked & RW_BIT(RW_ITEM_BLANKET_BASIC));
    TEST_ASSERT_FALSE(s.unlocked & RW_BIT(RW_ITEM_SANTA));
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_HAT_BOY_BASIC, s.hat_boy);
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_HAT_GIRL_BASIC, s.hat_girl);
    TEST_ASSERT_EQUAL_UINT8(RW_ITEM_BLANKET_BASIC, s.blanket);

    v1.version = 0;
    TEST_ASSERT_FALSE(rw_migrate_v1(&s, &v1));
    v1.version = 1; v1.n_countries = RW_PASSPORT_MAX + 1;
    TEST_ASSERT_FALSE(rw_migrate_v1(&s, &v1));
    v1.n_countries = 0; v1.seals = 0;
    TEST_ASSERT_TRUE(rw_migrate_v1(&s, &v1));
}

TEST_CASE("tienda: el guardado v1 no cambia de tamano", "[rewards]") {
    TEST_ASSERT_EQUAL(RW_STATE_V1_SIZE, sizeof(rw_state_v1_t));
}
