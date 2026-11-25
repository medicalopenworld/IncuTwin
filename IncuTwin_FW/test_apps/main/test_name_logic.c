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

TEST_CASE("evicted_counter_never_reuses_key", "[baby_name]")
{
    /* revision final #1: el modo Acompanar recuerda mas incubadoras que contadores; al volver a
     * una cuyo contador se expulso, el bebe nuevo no puede heredar el nombre de otro */
    fresh();
    obs("A", "", TWIN_BABY_IN);
    bn_set_name(&st, key, "Lola");
    obs("A", "", TWIN_BABY_NONE);
    obs("A", "", TWIN_BABY_IN);
    bn_set_name(&st, key, "Teo");
    const char *others[] = { "B", "C", "D", "E" }; /* BN_INCS = 4: A se queda sin contador */
    for (int i = 0; i < 4; i++) obs(others[i], "", TWIN_BABY_IN);
    obs("A", "", TWIN_BABY_IN);
    TEST_ASSERT_EQUAL_STRING("", bn_name(&st, key));
    TEST_ASSERT_TRUE(bn_should_ask(&st, key));
}

static twin_snapshot_t snap(void)
{
    twin_snapshot_t s;
    memset(&s, 0, sizeof(s));
    s.link.paired = true;
    s.link.state_rx = true;
    strcpy(s.link.incubator_id, "297");
    s.inc.online = true;
    s.inc.baby = TWIN_BABY_IN;
    s.inc.thermo = TWIN_THERMO_STABLE;
    s.show_baby = true;
    return s;
}

TEST_CASE("observed_inc_needs_fresh_state", "[baby_name]")
{
    /* revision final #2: el snapshot reseteado de un cambio de emparejamiento (state_rx=false,
     * baby=none) no es una salida del bebe */
    twin_snapshot_t s = snap();
    TEST_ASSERT_EQUAL_STRING("297", bn_observed_inc(&s));
    s.link.state_rx = false;
    TEST_ASSERT_EQUAL_STRING("", bn_observed_inc(&s));
    s = snap();
    s.link.paired = false;
    TEST_ASSERT_EQUAL_STRING("", bn_observed_inc(&s));

    fresh();
    obs("297", "", TWIN_BABY_IN);
    bn_set_name(&st, key, "Lola");
    s = snap();
    s.link.state_rx = false;
    s.inc.baby = TWIN_BABY_NONE;
    obs(bn_observed_inc(&s), "", s.inc.baby);
    TEST_ASSERT_EQUAL_STRING("297#1", obs("297", "", TWIN_BABY_IN));
    TEST_ASSERT_EQUAL_STRING("Lola", bn_name(&st, key));
}

TEST_CASE("can_ask_only_when_it_does_not_bother", "[baby_name]")
{
    twin_snapshot_t s = snap();
    TEST_ASSERT_TRUE(bn_can_ask(&s));
    s = snap(); s.demo = true;                      TEST_ASSERT_FALSE(bn_can_ask(&s));
    s = snap(); s.show_baby = false;                TEST_ASSERT_FALSE(bn_can_ask(&s));
    s = snap(); s.inc.baby = TWIN_BABY_PARENTS;     TEST_ASSERT_FALSE(bn_can_ask(&s));
    s = snap(); s.inc.thermo = TWIN_THERMO_ALARM;   TEST_ASSERT_FALSE(bn_can_ask(&s));
    s = snap(); strcpy(s.inc.name, "Amara");        TEST_ASSERT_FALSE(bn_can_ask(&s));
}

TEST_CASE("open_dialog_closes_when_state_changes", "[baby_name]")
{
    /* revision final #3: el aviso o el teclado abiertos no tapan una alarma, no siguen en la
     * demo y no guardan el nombre en otro bebe */
    twin_snapshot_t s = snap();
    TEST_ASSERT_FALSE(bn_must_close(&s, "297#1", "297#1"));
    s.inc.baby = TWIN_BABY_PARENTS; /* canguro: el mismo bebe, se puede seguir escribiendo */
    TEST_ASSERT_FALSE(bn_must_close(&s, "297#1", "297#1"));
    s = snap(); s.inc.thermo = TWIN_THERMO_ALARM;   TEST_ASSERT_TRUE(bn_must_close(&s, "297#1", "297#1"));
    s = snap(); s.demo = true;                      TEST_ASSERT_TRUE(bn_must_close(&s, "297#1", "297#1"));
    s = snap(); s.show_baby = false;                TEST_ASSERT_TRUE(bn_must_close(&s, "297#1", "297#1"));
    s = snap(); strcpy(s.inc.name, "Amara");        TEST_ASSERT_TRUE(bn_must_close(&s, "297#1", "297#1"));
    s = snap();
    TEST_ASSERT_TRUE(bn_must_close(&s, "297#1", "327#1")); /* otra estancia */
    TEST_ASSERT_TRUE(bn_must_close(&s, "297#1", ""));
}

TEST_CASE("key_case_capitalizes_words", "[baby_name]")
{
    char out[5];
    bn_key_case("L", true, out);
    TEST_ASSERT_EQUAL_STRING("L", out);
    bn_key_case("L", false, out);
    TEST_ASSERT_EQUAL_STRING("l", out);
    bn_key_case("Ñ", false, out);
    TEST_ASSERT_EQUAL_STRING("ñ", out);
    bn_key_case("Á", false, out);
    TEST_ASSERT_EQUAL_STRING("á", out);
    bn_key_case("Ú", false, out);
    TEST_ASSERT_EQUAL_STRING("ú", out);
    bn_key_case("É", true, out);
    TEST_ASSERT_EQUAL_STRING("É", out);
    /* mayuscula al empezar y tras un espacio */
    TEST_ASSERT_TRUE(bn_upper_next(""));
    TEST_ASSERT_FALSE(bn_upper_next("Mar"));
    TEST_ASSERT_TRUE(bn_upper_next("José "));
    TEST_ASSERT_FALSE(bn_upper_next("José M"));
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
