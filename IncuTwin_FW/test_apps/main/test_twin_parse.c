/* Spec twin-state-ingest: validacion del payload de incubators/<id>/state. */
#include <string.h>

#include "twin_logic.h"
#include "unity.h"

static twin_parse_result_t parse(const char *json, const char *expected, twin_msg_t *m)
{
    return twin_parse(json, strlen(json), expected, m);
}

TEST_CASE("parse: payload del contrato", "[twin][parse]")
{
    twin_msg_t m;
    const char *j = "{\"incubator_id\":\"353\",\"ts\":1759320000,\"state\":\"baby\","
                    "\"treatments\":[\"heat\",\"phototherapy\",\"pulseox\"],\"bpm\":142,"
                    "\"last_seen\":1759319940,\"event_seq\":1287,\"last_event\":\"treatment_changed\"}";
    TEST_ASSERT_EQUAL(TWIN_PARSE_OK, parse(j, "353", &m));
    TEST_ASSERT_EQUAL_STRING("353", m.incubator_id);
    TEST_ASSERT_EQUAL(TWIN_MSG_STATE_BABY, m.state);
    TEST_ASSERT_TRUE(m.heat);
    TEST_ASSERT_TRUE(m.photo);
    TEST_ASSERT_TRUE(m.pulseox);
    TEST_ASSERT_EQUAL(142, m.bpm);
    TEST_ASSERT_TRUE(m.has_seq);
    TEST_ASSERT_EQUAL(1287, m.seq);
    TEST_ASSERT_EQUAL_STRING("treatment_changed", m.last_event);
    TEST_ASSERT_FALSE(m.has_thermo);
    TEST_ASSERT_FALSE(m.has_name);
}

TEST_CASE("parse: bpm null y campos ausentes", "[twin][parse]")
{
    twin_msg_t m;
    const char *j = "{\"incubator_id\":\"353\",\"state\":\"free\",\"bpm\":null}";
    TEST_ASSERT_EQUAL(TWIN_PARSE_OK, parse(j, "353", &m));
    TEST_ASSERT_EQUAL(TWIN_MSG_STATE_FREE, m.state);
    TEST_ASSERT_EQUAL(-1, m.bpm);
    TEST_ASSERT_FALSE(m.has_seq);
    TEST_ASSERT_FALSE(m.heat);
    TEST_ASSERT_EQUAL_STRING("", m.last_event);
}

TEST_CASE("parse: incubadora equivocada", "[twin][parse]")
{
    twin_msg_t m;
    TEST_ASSERT_EQUAL(TWIN_PARSE_BAD_ID,
                      parse("{\"incubator_id\":\"354\",\"state\":\"baby\"}", "353", &m));
    TEST_ASSERT_EQUAL(TWIN_PARSE_BAD_ID, parse("{\"state\":\"baby\"}", "353", &m));
    TEST_ASSERT_EQUAL(TWIN_PARSE_BAD_ID,
                      parse("{\"incubator_id\":\"35/3\",\"state\":\"baby\"}", "", &m));
    /* sin incubadora esperada (sim) se acepta cualquiera valida */
    TEST_ASSERT_EQUAL(TWIN_PARSE_OK, parse("{\"incubator_id\":\"SIM\",\"state\":\"baby\"}", "", &m));
}

TEST_CASE("parse: json roto, no objeto, estado invalido, demasiado grande", "[twin][parse]")
{
    twin_msg_t m;
    TEST_ASSERT_EQUAL(TWIN_PARSE_BAD_JSON, parse("{\"incubator_id\":\"353\",\"state\":\"baby\",", "353", &m));
    TEST_ASSERT_EQUAL(TWIN_PARSE_NOT_OBJECT, parse("[1,2,3]", "353", &m));
    TEST_ASSERT_EQUAL(TWIN_PARSE_BAD_STATE, parse("{\"incubator_id\":\"353\",\"state\":\"sleeping\"}", "353", &m));
    TEST_ASSERT_EQUAL(TWIN_PARSE_BAD_STATE, parse("{\"incubator_id\":\"353\"}", "353", &m));
    static char big[1100];
    memset(big, ' ', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    TEST_ASSERT_EQUAL(TWIN_PARSE_TOO_BIG, twin_parse(big, sizeof(big) - 1, "353", &m));
}

TEST_CASE("parse: extensiones validas e invalidas", "[twin][parse]")
{
    twin_msg_t m;
    const char *j = "{\"incubator_id\":\"353\",\"state\":\"baby\",\"thermo\":\"alarm\","
                    "\"baby\":\"parents\",\"awake\":true,\"skin\":9,\"name\":\"Lucía\","
                    "\"weight_g\":1250,\"age_d\":3}";
    TEST_ASSERT_EQUAL(TWIN_PARSE_OK, parse(j, "353", &m));
    TEST_ASSERT_TRUE(m.has_thermo);
    TEST_ASSERT_EQUAL(TWIN_THERMO_ALARM, m.thermo);
    TEST_ASSERT_TRUE(m.has_baby);
    TEST_ASSERT_EQUAL(TWIN_BABY_PARENTS, m.baby);
    TEST_ASSERT_TRUE(m.has_awake);
    TEST_ASSERT_TRUE(m.awake);
    TEST_ASSERT_FALSE(m.has_skin); /* 9 fuera de rango: se ignora */
    TEST_ASSERT_TRUE(m.has_name);
    TEST_ASSERT_EQUAL_STRING("Lucía", m.name);
    TEST_ASSERT_TRUE(m.has_weight);
    TEST_ASSERT_EQUAL(1250, m.weight_g);
    TEST_ASSERT_TRUE(m.has_age);
    TEST_ASSERT_EQUAL(3, m.age_d);
}

TEST_CASE("parse: nombre demasiado largo se ignora, vacio no cuenta", "[twin][parse]")
{
    twin_msg_t m;
    const char *j = "{\"incubator_id\":\"353\",\"state\":\"baby\","
                    "\"name\":\"NombreDemasiadoLargoParaLaBarra\"}";
    TEST_ASSERT_EQUAL(TWIN_PARSE_OK, parse(j, "353", &m));
    TEST_ASSERT_FALSE(m.has_name);
    TEST_ASSERT_EQUAL(TWIN_PARSE_OK,
                      parse("{\"incubator_id\":\"353\",\"state\":\"baby\",\"name\":\"\"}", "353", &m));
    TEST_ASSERT_FALSE(m.has_name);
}

TEST_CASE("parse: tratamientos desconocidos se ignoran", "[twin][parse]")
{
    twin_msg_t m;
    const char *j = "{\"incubator_id\":\"353\",\"state\":\"baby\",\"treatments\":[\"heat\",\"laser\",42]}";
    TEST_ASSERT_EQUAL(TWIN_PARSE_OK, parse(j, "353", &m));
    TEST_ASSERT_TRUE(m.heat);
    TEST_ASSERT_FALSE(m.photo);
    TEST_ASSERT_FALSE(m.pulseox);
}
