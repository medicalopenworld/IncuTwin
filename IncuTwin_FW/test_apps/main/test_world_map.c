/* Mapa de la pagina del pais: encuadre y relleno de tierra (components/world_map). */
#include "twin_geo.h"
#include "unity.h"
#include "world_map.h"

static const wm_frame_t F = { .w = 240, .h = 240, .top = 48, .bottom = 68, .margin = 16,
                              .min_ppd = 1.2f, .max_ppd = 9.0f };

static void assert_inside(const wm_view_t *v, float lat, float lon)
{
    float x, y;
    wm_project(v, lat, lon, &x, &y);
    TEST_ASSERT_TRUE(x >= F.margin - 0.5f && x <= F.w - F.margin + 0.5f);
    TEST_ASSERT_TRUE(y >= F.top + F.margin - 0.5f && y <= F.h - F.bottom - F.margin + 0.5f);
}

TEST_CASE("map: los dos puntos caben entre las bandas", "[map]")
{
    static const char *CC[] = { "TG", "PH", "MX", "KE", "MA", "PE" };
    for (int i = 0; i < 6; i++) {
        const twin_geo_t *g = twin_geo_find(CC[i]);
        wm_view_t v;
        wm_fit(&v, &F, TWIN_GEO_HOME_LAT, TWIN_GEO_HOME_LON, g->lat, g->lon);
        assert_inside(&v, TWIN_GEO_HOME_LAT, TWIN_GEO_HOME_LON);
        assert_inside(&v, g->lat, g->lon);
    }
}

TEST_CASE("map: zoom maximo con el mismo punto", "[map]")
{
    wm_view_t v;
    wm_fit(&v, &F, 40.4f, -3.7f, 40.4f, -3.7f);
    TEST_ASSERT_EQUAL_FLOAT(9.0f, v.sy);
    float x, y;
    wm_project(&v, 40.4f, -3.7f, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 120.0f, x);
}

TEST_CASE("map: Sahara es tierra y el Atlantico mar", "[map]")
{
    static uint16_t buf[8 * 8];
    const uint16_t LAND = 0xAAAA, SEA = 0x5555;
    wm_view_t v = { .sx = 2.0f, .sy = 2.0f };
    v.lon0 = 10.0f - 2.0f; v.lat0 = 23.0f + 2.0f; /* 8x8 px alrededor de 23 N 10 E */
    wm_render(buf, 8, 8, &v, LAND, SEA);
    for (int i = 0; i < 64; i++) TEST_ASSERT_EQUAL_HEX16(LAND, buf[i]);
    v.lon0 = -40.0f - 2.0f; v.lat0 = 30.0f + 2.0f;
    wm_render(buf, 8, 8, &v, LAND, SEA);
    for (int i = 0; i < 64; i++) TEST_ASSERT_EQUAL_HEX16(SEA, buf[i]);
}
