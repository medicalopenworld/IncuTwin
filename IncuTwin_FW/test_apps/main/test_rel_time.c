/* Historial (spec hmi-navegacion-avisos): tiempo relativo de cada aviso (funcion pura de ui). */
#include "ui_rel_time.h"
#include "unity.h"

TEST_CASE("rel_time: tramos", "[ui]")
{
    char b[24];
    ui_rel_time(1000, 1030, b, sizeof b, 0); TEST_ASSERT_EQUAL_STRING("ahora", b);
    ui_rel_time(1000, 1000 + 5 * 60, b, sizeof b, 0); TEST_ASSERT_EQUAL_STRING("hace 5 min", b);
    ui_rel_time(1000, 1000 + 2 * 3600, b, sizeof b, 0); TEST_ASSERT_EQUAL_STRING("hace 2 h", b);
    ui_rel_time(1000, 1000 + 3 * 86400, b, sizeof b, 1); TEST_ASSERT_EQUAL_STRING("3 d ago", b);
    /* sin hora: guion ASCII (las fuentes no tienen la raya U+2014) */
    ui_rel_time(0, 5000, b, sizeof b, 0); TEST_ASSERT_EQUAL_STRING("-", b);
}

TEST_CASE("rel_time: bordes y reloj hacia atras", "[ui]")
{
    char b[24];
    ui_rel_time(1000, 1059, b, sizeof b, 0); TEST_ASSERT_EQUAL_STRING("ahora", b);
    ui_rel_time(1000, 1060, b, sizeof b, 1); TEST_ASSERT_EQUAL_STRING("1 min ago", b);
    ui_rel_time(1000, 1000 + 3599, b, sizeof b, 0); TEST_ASSERT_EQUAL_STRING("hace 59 min", b);
    ui_rel_time(1000, 1000 + 86399, b, sizeof b, 1); TEST_ASSERT_EQUAL_STRING("23 h ago", b);
    ui_rel_time(1000, 1000 + 86400, b, sizeof b, 0); TEST_ASSERT_EQUAL_STRING("hace 1 d", b);
    ui_rel_time(1000, 1030, b, sizeof b, 1); TEST_ASSERT_EQUAL_STRING("now", b);
    ui_rel_time(5000, 1000, b, sizeof b, 0); TEST_ASSERT_EQUAL_STRING("ahora", b); /* reloj corregido */
    ui_rel_time(1000, 5000, b, 4, 0); TEST_ASSERT_EQUAL_STRING("hac", b);         /* recorta sin desbordar */
}
