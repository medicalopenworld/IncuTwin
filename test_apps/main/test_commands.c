/* Specs device-commands y pairing: topics y payloads de comandos. */
#include <string.h>

#include "commands_parse.h"
#include "unity.h"

#define CID "incutwin-2640-0007"

TEST_CASE("topic: unicast, flota y rechazos", "[cmd]")
{
    char name[CMD_NAME_MAX];
    TEST_ASSERT_EQUAL(CMD_TOPIC_UNICAST, cmd_parse_topic("incutwin/" CID "/cmd/ota", CID, name));
    TEST_ASSERT_EQUAL_STRING("ota", name);
    TEST_ASSERT_EQUAL(CMD_TOPIC_FLEET, cmd_parse_topic("incutwin/all/cmd/reboot", CID, name));
    TEST_ASSERT_EQUAL_STRING("reboot", name);
    TEST_ASSERT_EQUAL(CMD_TOPIC_NONE, cmd_parse_topic("incutwin/" CID "/cmd/ota/extra", CID, name));
    TEST_ASSERT_EQUAL(CMD_TOPIC_NONE, cmd_parse_topic("incutwin/" CID "/cmd/", CID, name));
    TEST_ASSERT_EQUAL(CMD_TOPIC_NONE, cmd_parse_topic("incutwin/otro-panel/cmd/ota", CID, name));
    TEST_ASSERT_EQUAL(CMD_TOPIC_NONE, cmd_parse_topic("incutwin/" CID "/status", CID, name));
    TEST_ASSERT_EQUAL(CMD_TOPIC_NONE, cmd_parse_topic("incubators/353/state", CID, name));
}

TEST_CASE("topic: incubators/<id>/state", "[cmd]")
{
    char id[TWIN_INCUBATOR_ID_LEN];
    TEST_ASSERT_TRUE(cmd_is_state_topic("incubators/353/state", id));
    TEST_ASSERT_EQUAL_STRING("353", id);
    TEST_ASSERT_TRUE(cmd_is_state_topic("incubators/IncuNest-1_2/state", NULL));
    TEST_ASSERT_FALSE(cmd_is_state_topic("incubators/353/state/x", NULL));
    TEST_ASSERT_FALSE(cmd_is_state_topic("incubators//state", NULL));
    TEST_ASSERT_FALSE(cmd_is_state_topic("incubators/353", NULL));
    TEST_ASSERT_FALSE(cmd_is_state_topic("incutwin/all/cmd/ota", NULL));
}

TEST_CASE("pair: set, unpair e invalidos", "[pairing]")
{
    char id[TWIN_INCUBATOR_ID_LEN];
    const char *ok = "{\"incubator_id\":\"353\"}";
    TEST_ASSERT_EQUAL(PAIR_SET, cmd_parse_pair(ok, strlen(ok), id));
    TEST_ASSERT_EQUAL_STRING("353", id);
    TEST_ASSERT_EQUAL(PAIR_UNPAIR, cmd_parse_pair("", 0, id));
    const char *bad1 = "{\"incubator_id\":\"353/state\"}";
    TEST_ASSERT_EQUAL(PAIR_INVALID, cmd_parse_pair(bad1, strlen(bad1), id));
    const char *bad2 = "{\"incubator\":\"353\"}";
    TEST_ASSERT_EQUAL(PAIR_INVALID, cmd_parse_pair(bad2, strlen(bad2), id));
    const char *bad3 = "353";
    TEST_ASSERT_EQUAL(PAIR_INVALID, cmd_parse_pair(bad3, strlen(bad3), id));
    const char *bad4 = "{\"incubator_id\":";
    TEST_ASSERT_EQUAL(PAIR_INVALID, cmd_parse_pair(bad4, strlen(bad4), id));
}

TEST_CASE("brightness: clamp 10..100 y rechazo", "[cmd]")
{
    int v;
    const char *a = "{\"value\": 40}";
    TEST_ASSERT_TRUE(cmd_parse_brightness(a, strlen(a), &v));
    TEST_ASSERT_EQUAL(40, v);
    const char *b = "{\"value\": 0}";
    TEST_ASSERT_TRUE(cmd_parse_brightness(b, strlen(b), &v));
    TEST_ASSERT_EQUAL(10, v);
    const char *c = "{\"value\": 250}";
    TEST_ASSERT_TRUE(cmd_parse_brightness(c, strlen(c), &v));
    TEST_ASSERT_EQUAL(100, v);
    const char *d = "{\"value\": \"alto\"}";
    TEST_ASSERT_FALSE(cmd_parse_brightness(d, strlen(d), &v));
    TEST_ASSERT_FALSE(cmd_parse_brightness("{}", 2, &v));
}

TEST_CASE("test_melody: seleccion opcional", "[cmd]")
{
    char m[16];
    const char *a = "{\"melody\":\"parents\"}";
    cmd_parse_melody(a, strlen(a), m);
    TEST_ASSERT_EQUAL_STRING("parents", m);
    cmd_parse_melody("", 0, m);
    TEST_ASSERT_EQUAL_STRING("", m);
    const char *b = "{\"melody\":\"disco\"}";
    cmd_parse_melody(b, strlen(b), m);
    TEST_ASSERT_EQUAL_STRING("", m);
}
