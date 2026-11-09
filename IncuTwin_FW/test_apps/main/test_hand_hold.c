/* Spec hold-my-hand: estados, gracia, espera entre avisos. Relojes sinteticos en ms. */
#include <string.h>

#include "hand_hold.h"
#include "hand_payload.h"
#include "unity.h"

#define S(x) ((uint32_t)(x) * 1000u)

static hand_hold_t h;

static void setup(uint32_t cooldown_s)
{
    const hh_config_t cfg = { .min_ms = S(60), .grace_ms = S(3), .ready_ms = S(20),
                              .thanks_ms = S(4), .cooldown_ms = S(cooldown_s) };
    hh_init(&h, &cfg);
    hh_set_available(&h, true, true, 0);
}

/* dedo puesto de t0 a t1, con ticks cada 100 ms */
static void hold(uint32_t t0, uint32_t t1)
{
    hh_touch(&h, true, t0);
    for (uint32_t t = t0; t <= t1; t += 100) hh_tick(&h, t);
    hh_touch(&h, false, t1);
}

static void idle_until(uint32_t t0, uint32_t t1)
{
    for (uint32_t t = t0; t <= t1; t += 100) hh_tick(&h, t);
}

TEST_CASE("hand_hold: gesto completo con una pausa de 2 s", "[hand_hold]")
{
    setup(0);
    hh_button(&h, 0);
    TEST_ASSERT_EQUAL(HH_READY, hh_state(&h));
    hold(S(1), S(36)); /* 35 s */
    TEST_ASSERT_EQUAL(HH_GRACE, hh_state(&h));
    TEST_ASSERT_FALSE(hh_take_send(&h));
    idle_until(S(36), S(38));
    TEST_ASSERT_EQUAL(HH_GRACE, hh_state(&h));
    TEST_ASSERT_EQUAL_UINT8(1, hh_grace_left_s(&h, S(38)));
    hh_touch(&h, true, S(38));
    TEST_ASSERT_EQUAL(HH_HOLDING, hh_state(&h));
    idle_until(S(38), S(62)); /* 24 s mas: 59 s */
    TEST_ASSERT_EQUAL(HH_HOLDING, hh_state(&h));
    hh_tick(&h, S(63)); /* 60 s */
    TEST_ASSERT_EQUAL(HH_DONE, hh_state(&h));
    TEST_ASSERT_EQUAL_UINT16(1000, hh_progress_permille(&h, S(63)));
    TEST_ASSERT_TRUE(hh_take_send(&h));
    TEST_ASSERT_FALSE(hh_take_send(&h));
}

TEST_CASE("hand_hold: progreso solo con el dedo puesto", "[hand_hold]")
{
    setup(0);
    hold(0, S(30));
    TEST_ASSERT_EQUAL_UINT16(500, hh_progress_permille(&h, S(35)));
}

TEST_CASE("hand_hold: pausa de 3 s vuelve a reposo sin aviso", "[hand_hold]")
{
    setup(0);
    hold(0, S(40));
    idle_until(S(40), S(42));
    TEST_ASSERT_EQUAL(HH_GRACE, hh_state(&h));
    idle_until(S(42), S(50));
    TEST_ASSERT_EQUAL(HH_IDLE, hh_state(&h));
    TEST_ASSERT_EQUAL_UINT16(0, hh_progress_permille(&h, S(50)));
    TEST_ASSERT_FALSE(hh_take_send(&h));
    hold(S(51), S(52)); /* empieza de cero */
    TEST_ASSERT_EQUAL_UINT16(16, hh_progress_permille(&h, S(52)));
}

TEST_CASE("hand_hold: listo sin tocar 20 s vuelve a reposo", "[hand_hold]")
{
    setup(0);
    hh_button(&h, 0);
    idle_until(0, S(19));
    TEST_ASSERT_EQUAL(HH_READY, hh_state(&h));
    hh_tick(&h, S(20));
    TEST_ASSERT_EQUAL(HH_IDLE, hh_state(&h));
}

TEST_CASE("hand_hold: seguir tras completar no reenvia", "[hand_hold]")
{
    setup(0);
    hh_touch(&h, true, 0);
    idle_until(0, S(240));
    TEST_ASSERT_EQUAL(HH_DONE, hh_state(&h));
    TEST_ASSERT_TRUE(hh_take_send(&h));
    TEST_ASSERT_FALSE(hh_take_send(&h));
    hh_touch(&h, false, S(240));
    TEST_ASSERT_EQUAL(HH_THANKS, hh_state(&h));
    hh_touch(&h, true, S(241)); /* vuelve el dedo durante el agradecimiento */
    TEST_ASSERT_EQUAL(HH_DONE, hh_state(&h));
    hh_touch(&h, false, S(242));
    idle_until(S(242), S(246));
    TEST_ASSERT_EQUAL(HH_IDLE, hh_state(&h));
    TEST_ASSERT_FALSE(hh_take_send(&h));
}

TEST_CASE("hand_hold: tras completar el anillo sigue dando vueltas", "[hand_hold]")
{
    setup(0);
    hh_touch(&h, true, 0);
    idle_until(0, S(30));
    TEST_ASSERT_EQUAL_UINT32(S(30), hh_held_ms(&h, S(30)));
    idle_until(S(30), S(150)); /* 2,5 vueltas */
    TEST_ASSERT_EQUAL(HH_DONE, hh_state(&h));
    TEST_ASSERT_EQUAL_UINT32(2, hh_held_ms(&h, S(150)) / S(60));
    TEST_ASSERT_UINT32_WITHIN(150, S(30), hh_held_ms(&h, S(150)) % S(60));
    TEST_ASSERT_TRUE(hh_take_send(&h));
    hh_touch(&h, false, S(150));
    TEST_ASSERT_EQUAL(HH_THANKS, hh_state(&h));
    /* al agradecer el anillo se queda quieto */
    uint32_t frozen = hh_held_ms(&h, S(151));
    TEST_ASSERT_EQUAL_UINT32(frozen, hh_held_ms(&h, S(153)));
    /* vuelve el dedo: sigue sumando desde ahi, sin segundo aviso */
    hh_touch(&h, true, S(153));
    idle_until(S(153), S(163));
    TEST_ASSERT_UINT32_WITHIN(150, frozen + S(10), hh_held_ms(&h, S(163)));
    TEST_ASSERT_FALSE(hh_take_send(&h));
}

TEST_CASE("hand_hold: en reposo no hay tiempo acumulado", "[hand_hold]")
{
    setup(0);
    TEST_ASSERT_EQUAL_UINT32(0, hh_held_ms(&h, S(5)));
    hold(0, S(20));
    TEST_ASSERT_EQUAL_UINT32(S(20), hh_held_ms(&h, S(22))); /* gracia: congelado */
    idle_until(S(20), S(31));
    TEST_ASSERT_EQUAL(HH_IDLE, hh_state(&h));
    TEST_ASSERT_EQUAL_UINT32(0, hh_held_ms(&h, S(31)));
}

TEST_CASE("hand_hold: tocar directamente desde reposo", "[hand_hold]")
{
    setup(0);
    hh_touch(&h, true, 0);
    TEST_ASSERT_EQUAL(HH_HOLDING, hh_state(&h));
}

TEST_CASE("hand_hold: sin envio en demo o sin sesion", "[hand_hold]")
{
    setup(0);
    hh_set_available(&h, true, false, 0);
    hold(0, S(61));
    TEST_ASSERT_EQUAL(HH_THANKS, hh_state(&h));
    TEST_ASSERT_FALSE(hh_take_send(&h));
}

TEST_CASE("hand_hold: la vista desaparece y vuelve a reposo", "[hand_hold]")
{
    setup(0);
    hh_touch(&h, true, 0);
    idle_until(0, S(30));
    hh_set_available(&h, false, true, S(30));
    TEST_ASSERT_EQUAL(HH_IDLE, hh_state(&h));
    TEST_ASSERT_EQUAL_UINT16(0, hh_progress_permille(&h, S(31)));
}

TEST_CASE("hand_hold: espera 0 publica dos gestos seguidos", "[hand_hold]")
{
    setup(0);
    hold(0, S(61));
    TEST_ASSERT_TRUE(hh_take_send(&h));
    idle_until(S(61), S(66));
    hold(S(70), S(131));
    TEST_ASSERT_TRUE(hh_take_send(&h));
}

TEST_CASE("hand_hold: espera de 1800 s", "[hand_hold]")
{
    setup(1800);
    hold(0, S(61)); /* aviso a los 60 s */
    TEST_ASSERT_TRUE(hh_take_send(&h));
    idle_until(S(61), S(66));
    hold(S(600), S(661)); /* 10 min despues */
    TEST_ASSERT_FALSE(hh_take_send(&h));
    idle_until(S(661), S(666));
    hold(S(1800), S(1861)); /* completa a los 1860 s: 30 min desde el primero */
    TEST_ASSERT_TRUE(hh_take_send(&h));
}

TEST_CASE("hand_hold: un gesto en demo no consume la espera", "[hand_hold]")
{
    setup(1800);
    hh_set_available(&h, true, false, 0);
    hold(0, S(61));
    TEST_ASSERT_FALSE(hh_take_send(&h));
    idle_until(S(61), S(66));
    hh_set_available(&h, true, true, S(66));
    hold(S(70), S(131));
    TEST_ASSERT_TRUE(hh_take_send(&h));
}

TEST_CASE("hand_hold: reloj que desborda 32 bits", "[hand_hold]")
{
    setup(0);
    uint32_t t0 = 0xFFFFFFFFu - S(30);
    hh_touch(&h, true, t0);
    for (uint32_t i = 0; i <= 610; i++) hh_tick(&h, t0 + i * 100u);
    TEST_ASSERT_EQUAL(HH_DONE, hh_state(&h));
}

TEST_CASE("hand payload: JSON exacto", "[hand_hold]")
{
    char buf[96];
    int n = hand_payload_build(buf, sizeof(buf), "353", 60);
    TEST_ASSERT_EQUAL_STRING("{\"hand_hold\":1,\"incubator_id\":\"353\",\"hold_s\":60}", buf);
    TEST_ASSERT_EQUAL_INT((int)strlen(buf), n);
    TEST_ASSERT_EQUAL_INT(-1, hand_payload_build(buf, 10, "353", 60));
}
