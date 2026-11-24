#include "unity.h"
#include "quiet_hours.h"

TEST_CASE("quiet: 21-9 por defecto", "[quiet]")
{
    TEST_ASSERT_FALSE(quiet_hours_active(20, true, 21, 9));
    TEST_ASSERT_TRUE(quiet_hours_active(21, true, 21, 9));
    TEST_ASSERT_TRUE(quiet_hours_active(0, true, 21, 9));
    TEST_ASSERT_TRUE(quiet_hours_active(8, true, 21, 9));
    TEST_ASSERT_FALSE(quiet_hours_active(9, true, 21, 9));
}

TEST_CASE("quiet: apagado, sin hora o from==to nunca", "[quiet]")
{
    TEST_ASSERT_FALSE(quiet_hours_active(23, false, 21, 9));
    TEST_ASSERT_FALSE(quiet_hours_active(-1, true, 21, 9));
    TEST_ASSERT_FALSE(quiet_hours_active(10, true, 10, 10));
}

TEST_CASE("quiet: intervalo sin medianoche", "[quiet]")
{
    TEST_ASSERT_TRUE(quiet_hours_active(14, true, 13, 15));
    TEST_ASSERT_FALSE(quiet_hours_active(15, true, 13, 15));
}
