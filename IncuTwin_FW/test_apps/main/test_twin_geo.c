/* Tabla de paises: tono de piel, hora y distancia (spec twin-display, pagina del pais). */
#include "twin_geo.h"
#include "twin_logic.h"
#include "unity.h"

TEST_CASE("geo: pais conocido y desconocido", "[twin][geo]")
{
    const twin_geo_t *tg = twin_geo_find("TG");
    TEST_ASSERT_NOT_NULL(tg);
    TEST_ASSERT_EQUAL_INT16(0, tg->utc_min);
    TEST_ASSERT_EQUAL_UINT8(4, tg->skin);
    TEST_ASSERT_EQUAL_INT16(60, twin_geo_find("CM")->utc_min);
    TEST_ASSERT_NULL(twin_geo_find("XX"));
    TEST_ASSERT_NULL(twin_geo_find(""));
    TEST_ASSERT_NULL(twin_geo_find(NULL));
}

TEST_CASE("geo: tono 4 de 5 si no se sabe el pais", "[twin][geo]")
{
    TEST_ASSERT_EQUAL_UINT8(3, twin_geo_skin(""));
    TEST_ASSERT_EQUAL_UINT8(3, twin_geo_skin("ZZ"));
    TEST_ASSERT_EQUAL_UINT8(4, twin_geo_skin("CM"));
    TEST_ASSERT_EQUAL_UINT8(1, twin_geo_skin("ES"));
}

TEST_CASE("geo: distancia Madrid - Togo", "[twin][geo]")
{
    const twin_geo_t *tg = twin_geo_find("TG");
    uint32_t km = twin_geo_distance_km(TWIN_GEO_HOME_LAT, TWIN_GEO_HOME_LON, tg->lat, tg->lon);
    TEST_ASSERT_UINT32_WITHIN(80, 3640, km);
    TEST_ASSERT_EQUAL_UINT32(0, twin_geo_distance_km(10, 20, 10, 20));
}

static twin_msg_t msg_country(uint32_t seq, const char *cc)
{
    twin_msg_t m = { 0 };
    m.state = TWIN_MSG_STATE_BABY;
    m.has_seq = true;
    m.seq = seq;
    m.bpm = -1;
    if (cc) {
        m.has_country = true;
        m.country[0] = cc[0];
        m.country[1] = cc[1];
        m.country[2] = '\0';
    }
    return m;
}

TEST_CASE("apply: el tono sale del pais si no viene skin", "[twin][geo]")
{
    twin_incubator_t inc; uint32_t seq = 0;
    twin_incubator_reset(&inc, 1);
    twin_msg_t m = msg_country(1, "TG");
    twin_apply(&inc, &seq, &m);
    TEST_ASSERT_EQUAL_UINT8(4, inc.skin);
    m = msg_country(2, NULL); /* sin pais: 4 de 5 */
    twin_apply(&inc, &seq, &m);
    TEST_ASSERT_EQUAL_UINT8(3, inc.skin);
    m = msg_country(3, "UA");
    m.has_skin = true; m.skin = 2; /* skin explicito manda y se conserva */
    twin_apply(&inc, &seq, &m);
    TEST_ASSERT_EQUAL_UINT8(2, inc.skin);
    m = msg_country(4, "TG");
    twin_apply(&inc, &seq, &m);
    TEST_ASSERT_EQUAL_UINT8(2, inc.skin);
}
