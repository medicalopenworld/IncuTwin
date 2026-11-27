/* Caricia y cosquillas a partir de los puntos del dedo. Relojes sinteticos en ms. */
#include "affection.h"
#include "rewards_logic.h"
#include "unity.h"

/* Recorre el eje x a 100 de alto, un punto cada 20 ms. */
static aff_kind_t swipe(aff_stroke_t *s, int x0, int x1, int step, uint32_t *t, rw_level_t lv) {
    aff_kind_t k = AFF_NONE;
    for (int x = x0; step > 0 ? x <= x1 : x >= x1; x += step) { *t += 20; k = aff_move(s, x, 100, *t, lv); }
    return k;
}

TEST_CASE("affection: lo que abre cada nivel", "[affection]") {
    TEST_ASSERT_FALSE(aff_open(RW_LEVEL_HAND, AFF_CARESS));
    TEST_ASSERT_TRUE(aff_open(RW_LEVEL_CARESS, AFF_CARESS));
    TEST_ASSERT_FALSE(aff_open(RW_LEVEL_CARESS, AFF_TICKLES));
    TEST_ASSERT_TRUE(aff_open(RW_LEVEL_SQUEEZE, AFF_TICKLES));
}

TEST_CASE("affection: un dedo quieto que tiembla no es caricia", "[affection]") {
    aff_stroke_t s = { 0 }; uint32_t t = 0; aff_down(&s, 100, 100, t);
    for (int i = 0; i < 3000; i++) { t += 20; TEST_ASSERT_EQUAL(AFF_NONE, aff_move(&s, 100 + (i % 3) - 1, 100 + (i % 2), t, RW_LEVEL_SQUEEZE)); }
}

TEST_CASE("affection: recorrer 40 px es caricia", "[affection]") {
    aff_stroke_t s = { 0 }; uint32_t t = 0; aff_down(&s, 100, 100, t);
    TEST_ASSERT_EQUAL(AFF_NONE, swipe(&s, 100, 130, 6, &t, RW_LEVEL_CARESS));
    TEST_ASSERT_EQUAL(AFF_CARESS, swipe(&s, 136, 148, 6, &t, RW_LEVEL_CARESS));
}

TEST_CASE("affection: en Manita acariciar no hace nada", "[affection]") {
    aff_stroke_t s = { 0 }; uint32_t t = 0; aff_down(&s, 100, 100, t);
    TEST_ASSERT_EQUAL(AFF_NONE, swipe(&s, 100, 200, 6, &t, RW_LEVEL_HAND));
}

TEST_CASE("affection: ida y vuelta rapida es cosquillas, y en Caricia se queda en caricia", "[affection]") {
    for (int pass = 0; pass < 2; pass++) {
        rw_level_t lv = pass ? RW_LEVEL_TICKLES : RW_LEVEL_CARESS;
        aff_stroke_t s = { 0 }; uint32_t t = 0; aff_down(&s, 100, 100, t);
        aff_kind_t k = AFF_NONE;
        for (int i = 0; i < 3; i++) { k = swipe(&s, 100, 142, 6, &t, lv); k = swipe(&s, 142, 100, -6, &t, lv); }
        TEST_ASSERT_EQUAL(pass ? AFF_TICKLES : AFF_CARESS, k);
    }
}

TEST_CASE("affection: ida y vuelta lenta es caricia, no cosquillas", "[affection]") {
    aff_stroke_t s = { 0 }; uint32_t t = 0; aff_down(&s, 100, 100, t);
    aff_kind_t k = AFF_NONE;
    for (int i = 0; i < 3; i++) { k = swipe(&s, 100, 142, 1, &t, RW_LEVEL_TICKLES); k = swipe(&s, 142, 100, -1, &t, RW_LEVEL_TICKLES); }
    TEST_ASSERT_EQUAL(AFF_CARESS, k);   /* 42 px a 20 ms/px: cada tramo dura 840 ms */
}

TEST_CASE("affection: soltar y volver a tocar empieza de cero", "[affection]") {
    aff_stroke_t s = { 0 }; uint32_t t = 0; aff_down(&s, 100, 100, t);
    swipe(&s, 100, 160, 6, &t, RW_LEVEL_CARESS); aff_up(&s);
    aff_down(&s, 100, 100, t);
    TEST_ASSERT_EQUAL(AFF_NONE, aff_move(&s, 104, 100, t + 20, RW_LEVEL_CARESS));
}
