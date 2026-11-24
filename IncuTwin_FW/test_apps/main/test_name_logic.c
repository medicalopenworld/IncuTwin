/* Spec nombre-bebe-panel: nombre local del bebe (logica pura de baby_name). */
#include <stdio.h>
#include <string.h>

#include "name_logic.h"
#include "unity.h"

static bn_store_t st;
static char key[BN_KEY_LEN];

static void fresh(void)
{
    bn_store_reset(&st);
    key[0] = '\0';
}

static const char *obs(const char *inc, const char *stay, twin_baby_t baby)
{
    bn_observe(&st, inc, stay, baby, key);
    return key;
}

TEST_CASE("first_baby_asks", "[baby_name]")
{
    fresh();
    TEST_ASSERT_EQUAL_STRING("297#1", obs("297", "", TWIN_BABY_IN));
    TEST_ASSERT_TRUE(bn_should_ask(&st, key));
    TEST_ASSERT_TRUE(bn_mark_asked(&st, key));
    TEST_ASSERT_FALSE(bn_should_ask(&st, key));
    TEST_ASSERT_FALSE(bn_mark_asked(&st, key)); /* ya estaba */
    TEST_ASSERT_EQUAL_STRING("", bn_name(&st, key));
}

TEST_CASE("kangaroo_keeps_stay", "[baby_name]")
{
    fresh();
    obs("297", "", TWIN_BABY_IN);
    TEST_ASSERT_TRUE(bn_set_name(&st, key, "Lola"));
    TEST_ASSERT_EQUAL_STRING("297#1", obs("297", "", TWIN_BABY_PARENTS));
    TEST_ASSERT_EQUAL_STRING("297#1", obs("297", "", TWIN_BABY_IN));
    TEST_ASSERT_EQUAL_STRING("Lola", bn_name(&st, key));
    TEST_ASSERT_FALSE(bn_should_ask(&st, key));
}

TEST_CASE("new_arrival_new_stay", "[baby_name]")
{
    fresh();
    obs("297", "", TWIN_BABY_IN);
    bn_set_name(&st, key, "Lola");
    obs("297", "", TWIN_BABY_NONE);
    TEST_ASSERT_EQUAL_STRING("297#2", obs("297", "", TWIN_BABY_IN));
    TEST_ASSERT_TRUE(bn_should_ask(&st, key));
    TEST_ASSERT_EQUAL_STRING("", bn_name(&st, key));
    /* alta a casa y otro bebe: tambien es otra estancia */
    obs("297", "", TWIN_BABY_HOME);
    TEST_ASSERT_EQUAL_STRING("297#3", obs("297", "", TWIN_BABY_IN));
}

TEST_CASE("reboot_mid_stay", "[baby_name]")
{
    fresh();
    obs("297", "", TWIN_BABY_IN);
    bn_mark_asked(&st, key);
    char before[BN_KEY_LEN];
    strcpy(before, key);
    bn_store_t saved;
    memcpy(&saved, &st, sizeof(st)); /* como si viniera de la NVS */
    memset(&st, 0xA5, sizeof(st));
    memcpy(&st, &saved, sizeof(st));
    TEST_ASSERT_FALSE(bn_observe(&st, "297", "", TWIN_BABY_IN, key));
    TEST_ASSERT_EQUAL_STRING(before, key);
    TEST_ASSERT_FALSE(bn_should_ask(&st, key));
}

TEST_CASE("two_incubators", "[baby_name]")
{
    fresh();
    obs("297", "", TWIN_BABY_IN);
    bn_set_name(&st, key, "Lola");
    TEST_ASSERT_EQUAL_STRING("327#1", obs("327", "", TWIN_BABY_IN));
    bn_set_name(&st, key, "Teo");
    TEST_ASSERT_EQUAL_STRING("297#1", obs("297", "", TWIN_BABY_IN));
    TEST_ASSERT_EQUAL_STRING("Lola", bn_name(&st, key));
    TEST_ASSERT_EQUAL_STRING("Teo", bn_name(&st, "327#1"));
}

TEST_CASE("ring_evicts_oldest", "[baby_name]")
{
    fresh();
    for (int i = 1; i <= BN_RING + 1; i++) {
        char name[8];
        snprintf(name, sizeof(name), "B%d", i);
        obs("297", "", TWIN_BABY_IN);
        TEST_ASSERT_TRUE(bn_set_name(&st, key, name));
        obs("297", "", TWIN_BABY_NONE);
    }
    TEST_ASSERT_EQUAL_STRING("", bn_name(&st, "297#1"));
    TEST_ASSERT_TRUE(bn_should_ask(&st, "297#1"));
    for (int i = 2; i <= BN_RING + 1; i++) {
        char k[BN_KEY_LEN], name[8];
        snprintf(k, sizeof(k), "297#%d", i);
        snprintf(name, sizeof(name), "B%d", i);
        TEST_ASSERT_EQUAL_STRING(name, bn_name(&st, k));
    }
}

TEST_CASE("ring_keeps_recently_used", "[baby_name]")
{
    fresh();
    for (int i = 1; i <= BN_RING; i++) {
        obs("297", "", TWIN_BABY_IN);
        bn_set_name(&st, key, "X");
        obs("297", "", TWIN_BABY_NONE);
    }
    bn_set_name(&st, "297#1", "Lola"); /* renombrar la mas antigua la hace la mas reciente */
    obs("297", "", TWIN_BABY_IN);
    bn_set_name(&st, key, "Nuevo");
    TEST_ASSERT_EQUAL_STRING("Lola", bn_name(&st, "297#1"));
    TEST_ASSERT_EQUAL_STRING("", bn_name(&st, "297#2"));
    TEST_ASSERT_EQUAL_STRING("Nuevo", bn_name(&st, key));
}

TEST_CASE("stay_id_keyed", "[baby_name]")
{
    fresh();
    TEST_ASSERT_EQUAL_STRING("297:4", obs("297", "297:4", TWIN_BABY_IN));
    TEST_ASSERT_TRUE(bn_should_ask(&st, key));
    bn_set_name(&st, key, "Lola");
    obs("297", "297:4", TWIN_BABY_PARENTS);
    TEST_ASSERT_EQUAL_STRING("297:4", obs("297", "297:4", TWIN_BABY_IN));
    TEST_ASSERT_EQUAL_STRING("Lola", bn_name(&st, key));
}

TEST_CASE("stay_id_adopts_fallback", "[baby_name]")
{
    fresh();
    obs("297", "", TWIN_BABY_IN);
    bn_set_name(&st, key, "Lola");
    TEST_ASSERT_TRUE(bn_observe(&st, "297", "297:4", TWIN_BABY_IN, key));
    TEST_ASSERT_EQUAL_STRING("297:4", key);
    TEST_ASSERT_EQUAL_STRING("Lola", bn_name(&st, key));
    TEST_ASSERT_FALSE(bn_should_ask(&st, key));
}

TEST_CASE("no_baby_no_key", "[baby_name]")
{
    fresh();
    TEST_ASSERT_EQUAL_STRING("", obs("297", "", TWIN_BABY_NONE));
    TEST_ASSERT_FALSE(bn_should_ask(&st, key));
    TEST_ASSERT_EQUAL_STRING("", obs("297", "297:4", TWIN_BABY_HOME));
    TEST_ASSERT_FALSE(bn_should_ask(&st, key));
    TEST_ASSERT_FALSE(bn_set_name(&st, "", "Lola"));
    TEST_ASSERT_EQUAL_STRING("", obs("", "", TWIN_BABY_IN)); /* sin incubadora */
}

TEST_CASE("clean_name_trims_and_clips", "[baby_name]")
{
    char out[BN_NAME_BYTES];
    TEST_ASSERT_TRUE(bn_clean_name("  José María de la Luz  ", out));
    TEST_ASSERT_EQUAL_STRING("José María d", out);
    TEST_ASSERT_TRUE(bn_clean_name("ÑÑÑÑÑÑÑÑÑÑÑÑÑÑ", out));
    TEST_ASSERT_EQUAL_STRING("ÑÑÑÑÑÑÑÑÑÑÑÑ", out);
    TEST_ASSERT_TRUE(bn_clean_name("日本語日本語日本語日本", out)); /* 3 bytes cada uno */
    TEST_ASSERT_EQUAL_STRING("日本語日本語日本", out);
    TEST_ASSERT_TRUE(bn_clean_name("Lola  ", out));
    TEST_ASSERT_EQUAL_STRING("Lola", out);
    TEST_ASSERT_FALSE(bn_clean_name("   ", out));
    TEST_ASSERT_FALSE(bn_clean_name("", out));
}

TEST_CASE("set_name_empty_keeps_previous", "[baby_name]")
{
    fresh();
    obs("297", "", TWIN_BABY_IN);
    TEST_ASSERT_TRUE(bn_set_name(&st, key, "Lola"));
    TEST_ASSERT_FALSE(bn_set_name(&st, key, "  "));
    TEST_ASSERT_EQUAL_STRING("Lola", bn_name(&st, key));
    TEST_ASSERT_TRUE(bn_set_name(&st, key, "Lucía"));
    TEST_ASSERT_EQUAL_STRING("Lucía", bn_name(&st, key));
}
