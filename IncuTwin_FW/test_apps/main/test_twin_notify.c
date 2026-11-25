/* Avisos de lo que vive el bebe (twin_notify): ingreso, padres, calor, fototerapia, salida. */
#include "twin_logic.h"
#include "twin_notify.h"
#include "unity.h"

static const twin_incubator_t *inc(twin_baby_t baby, twin_thermo_t thermo, bool photo, bool online)
{
    static twin_incubator_t s;
    twin_incubator_reset(&s, 1);
    s.online = online;
    s.baby = baby;
    s.thermo = thermo;
    s.photo = photo;
    return &s;
}

static twin_notice_base_t base(twin_baby_t baby, bool heat, bool photo)
{
    twin_notice_base_t b = { .valid = true, .treatments_known = true, .baby = baby, .heat = heat, .photo = photo };
    return b;
}

TEST_CASE("notice: primer estado solo fija la base", "[notice]")
{
    twin_notice_base_t b = { 0 };
    TEST_ASSERT_EQUAL_UINT32(0, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_STABLE, true, true)));
}

TEST_CASE("notice: arranque con bebe guardado no avisa de calor", "[notice]")
{
    twin_notice_base_t b;
    twin_notice_base_boot(&b, true, TWIN_BABY_IN);
    TEST_ASSERT_EQUAL_UINT32(0, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_STABLE, true, true)));
    TEST_ASSERT_EQUAL_UINT32(TWIN_NOTICE_PHOTO_OFF, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_STABLE, false, true)));
}

TEST_CASE("notice: arranque sin bebe guardado y llega bebe = ingreso", "[notice]")
{
    twin_notice_base_t b;
    twin_notice_base_boot(&b, true, TWIN_BABY_NONE);
    TEST_ASSERT_EQUAL_UINT32(TWIN_NOTICE_ADMISSION, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_HEATING, false, true)));
}

TEST_CASE("notice: ingreso con calor", "[notice]")
{
    twin_notice_base_t b = base(TWIN_BABY_NONE, false, false);
    TEST_ASSERT_EQUAL_UINT32(TWIN_NOTICE_ADMISSION | TWIN_NOTICE_HEAT_ON, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_HEATING, false, true)));
}

TEST_CASE("notice: in->parents avisa, parents->in no", "[notice]")
{
    twin_notice_base_t b = base(TWIN_BABY_IN, false, false);
    TEST_ASSERT_EQUAL_UINT32(TWIN_NOTICE_PARENTS, twin_notices_update(&b, inc(TWIN_BABY_PARENTS, TWIN_THERMO_OFF, false, true)));
    TEST_ASSERT_EQUAL_UINT32(0, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_OFF, false, true)));
}

TEST_CASE("notice: home es salida, sin fanfarria", "[notice]")
{
    twin_notice_base_t b = base(TWIN_BABY_PARENTS, false, false);
    TEST_ASSERT_EQUAL_UINT32(TWIN_NOTICE_EXIT, twin_notices_update(&b, inc(TWIN_BABY_HOME, TWIN_THERMO_OFF, false, true)));
}

TEST_CASE("notice: offline no avisa y al volver igual tampoco", "[notice]")
{
    twin_notice_base_t b = base(TWIN_BABY_IN, true, false);
    TEST_ASSERT_EQUAL_UINT32(0, twin_notices_update(&b, inc(TWIN_BABY_NONE, TWIN_THERMO_OFF, false, false)));
    TEST_ASSERT_EQUAL_UINT32(0, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_STABLE, false, true)));
}

TEST_CASE("notice: vuelve de offline sin bebe = una salida", "[notice]")
{
    twin_notice_base_t b = base(TWIN_BABY_IN, false, false);
    twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_OFF, false, false));
    TEST_ASSERT_EQUAL_UINT32(TWIN_NOTICE_EXIT, twin_notices_update(&b, inc(TWIN_BABY_NONE, TWIN_THERMO_OFF, false, true)));
}

TEST_CASE("notice: un \"alarm\" del broker es calor y no avisa si ya habia", "[notice]")
{
    twin_notice_base_t b = base(TWIN_BABY_IN, true, false);
    TEST_ASSERT_EQUAL_UINT32(0, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_HEATING, false, true)));
}
