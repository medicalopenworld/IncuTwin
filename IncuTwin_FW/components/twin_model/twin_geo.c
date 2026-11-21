#include "twin_geo.h"

#include <math.h>
#include <stddef.h>

/* Los paises de tr_country() (ui_i18n.c) mas Togo y Benin. Tono: 0 = mas claro .. 4 = mas
 * oscuro (Images/tones). Hora estandar; los pocos paises con horario de verano (ES, UA, PS,
 * HT) se quedan en invierno: es una referencia, no un reloj oficial. */
static const twin_geo_t GEO[] = {
    { "CM",   5.7f,   12.7f,   60, 4 }, { "TG",   8.6f,    0.9f,    0, 4 },
    { "BJ",   9.3f,    2.3f,    0, 4 }, { "GH",   7.9f,   -1.0f,    0, 4 },
    { "KE",   0.0f,   37.9f,  180, 4 }, { "SN",  14.5f,  -14.5f,    0, 4 },
    { "NG",   9.1f,    8.7f,   60, 4 }, { "CI",   7.5f,   -5.5f,    0, 4 },
    { "MA",  31.8f,   -7.1f,   60, 2 }, { "ET",   9.1f,   40.5f,  180, 3 },
    { "TZ",  -6.4f,   34.9f,  180, 4 }, { "UG",   1.4f,   32.3f,  180, 4 },
    { "MZ", -18.7f,   35.5f,  120, 4 }, { "CD",  -4.0f,   21.8f,   60, 4 },
    { "CG",  -0.2f,   15.8f,   60, 4 }, { "GA",  -0.8f,   11.6f,   60, 4 },
    { "GQ",   1.6f,   10.3f,   60, 4 }, { "GN",   9.9f,   -9.7f,    0, 4 },
    { "SL",   8.5f,  -11.8f,    0, 4 }, { "LR",   6.4f,   -9.4f,    0, 4 },
    { "ML",  17.6f,   -4.0f,    0, 4 }, { "BF",  12.2f,   -1.6f,    0, 4 },
    { "NE",  17.6f,    8.1f,   60, 4 }, { "TD",  15.5f,   18.7f,   60, 4 },
    { "CF",   6.6f,   20.9f,   60, 4 }, { "AO", -11.2f,   17.9f,   60, 4 },
    { "MW", -13.3f,   34.3f,  120, 4 }, { "ZM", -13.1f,   27.8f,  120, 4 },
    { "RW",  -1.9f,   29.9f,  120, 4 }, { "SS",   6.9f,   31.3f,  120, 4 },
    { "SD",  12.9f,   30.2f,  120, 4 }, { "SO",   5.2f,   46.2f,  180, 3 },
    { "MG", -18.8f,   46.9f,  180, 3 }, { "MX",  23.6f, -102.6f, -360, 2 },
    { "GT",  15.8f,  -90.2f, -360, 2 }, { "HN",  15.2f,  -86.2f, -360, 2 },
    { "NI",  12.9f,  -85.2f, -360, 2 }, { "HT",  19.0f,  -72.3f, -300, 4 },
    { "CO",   4.6f,  -74.3f, -300, 2 }, { "EC",  -1.8f,  -78.2f, -300, 2 },
    { "PE",  -9.2f,  -75.0f, -300, 2 }, { "BO", -16.3f,  -63.6f, -240, 2 },
    { "VE",   6.4f,  -66.6f, -240, 2 }, { "ES",  40.4f,   -3.7f,   60, 1 },
    { "UA",  48.4f,   31.2f,  120, 0 }, { "PS",  31.9f,   35.2f,  120, 1 },
    { "YE",  15.6f,   48.5f,  180, 2 }, { "AF",  33.9f,   67.7f,  270, 2 },
    { "IN",  20.6f,   79.0f,  330, 2 }, { "BD",  23.7f,   90.4f,  360, 2 },
    { "PK",  30.4f,   69.3f,  300, 2 }, { "PH",  12.9f,  121.8f,  480, 2 },
};

const twin_geo_t *twin_geo_find(const char *iso)
{
    if (!iso || !iso[0] || !iso[1]) return NULL;
    for (size_t i = 0; i < sizeof(GEO) / sizeof(GEO[0]); i++) {
        if (GEO[i].iso[0] == iso[0] && GEO[i].iso[1] == iso[1]) return &GEO[i];
    }
    return NULL;
}

uint8_t twin_geo_skin(const char *iso)
{
    const twin_geo_t *g = twin_geo_find(iso);
    return g ? g->skin : TWIN_SKIN_UNKNOWN;
}

uint32_t twin_geo_distance_km(float lat1, float lon1, float lat2, float lon2)
{
    const double R = 6371.0, D = M_PI / 180.0;
    double p1 = lat1 * D, p2 = lat2 * D, dp = (lat2 - lat1) * D, dl = (lon2 - lon1) * D;
    double a = sin(dp / 2) * sin(dp / 2) + cos(p1) * cos(p2) * sin(dl / 2) * sin(dl / 2);
    return (uint32_t)(2.0 * R * atan2(sqrt(a), sqrt(1.0 - a)) + 0.5);
}
