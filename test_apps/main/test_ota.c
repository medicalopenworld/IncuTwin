/* Spec ota-update: validacion del comando. */
#include <string.h>

#include "ota_validate.h"
#include "unity.h"

#define SUF ".medicalopenworld.org"
#define HASH "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

TEST_CASE("ota: url permitida", "[ota]")
{
    TEST_ASSERT_TRUE(ota_url_allowed("https://fw.medicalopenworld.org/incutwin/fw.bin", SUF, false));
    TEST_ASSERT_TRUE(ota_url_allowed("https://FW.MedicalOpenWorld.org:443/x.bin", SUF, false));
    TEST_ASSERT_FALSE(ota_url_allowed("http://fw.medicalopenworld.org/x.bin", SUF, false));
    TEST_ASSERT_FALSE(ota_url_allowed("https://evil.example.com/fw.bin", SUF, false));
    TEST_ASSERT_FALSE(ota_url_allowed("https://medicalopenworld.org.evil.com/fw.bin", SUF, false));
    TEST_ASSERT_FALSE(ota_url_allowed("https://.medicalopenworld.org/fw.bin", SUF, false)); /* sin host */
    TEST_ASSERT_TRUE(ota_url_allowed("https://abc.trycloudflare.com/fw.bin", SUF, true));
    TEST_ASSERT_FALSE(ota_url_allowed("http://abc.trycloudflare.com/fw.bin", SUF, true)); /* ni en dev sin TLS */
}

TEST_CASE("ota: sha256 hex", "[ota]")
{
    uint8_t out[32];
    TEST_ASSERT_TRUE(ota_parse_sha256(HASH, out));
    TEST_ASSERT_EQUAL_HEX8(0x01, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xef, out[31]);
    TEST_ASSERT_FALSE(ota_parse_sha256(HASH "0", out));
    TEST_ASSERT_FALSE(ota_parse_sha256("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeg", out));
    TEST_ASSERT_FALSE(ota_parse_sha256("", out));
}

TEST_CASE("ota: comando completo y rechazos", "[ota]")
{
    ota_job_t job;
    const char *ok = "{\"url\":\"https://fw.medicalopenworld.org/incutwin/incutwin-2.0.1.bin\",\"sha256\":\"" HASH "\"}";
    TEST_ASSERT_EQUAL(OTA_CMD_OK, ota_validate_cmd(ok, strlen(ok), SUF, false, &job));
    TEST_ASSERT_EQUAL_STRING("https://fw.medicalopenworld.org/incutwin/incutwin-2.0.1.bin", job.url);
    TEST_ASSERT_EQUAL_HEX8(0xef, job.sha256[31]);

    const char *bad_host = "{\"url\":\"https://evil.example.com/fw.bin\",\"sha256\":\"" HASH "\"}";
    TEST_ASSERT_EQUAL(OTA_CMD_BAD_URL, ota_validate_cmd(bad_host, strlen(bad_host), SUF, false, &job));
    const char *http = "{\"url\":\"http://fw.medicalopenworld.org/x.bin\",\"sha256\":\"" HASH "\"}";
    TEST_ASSERT_EQUAL(OTA_CMD_BAD_URL, ota_validate_cmd(http, strlen(http), SUF, false, &job));
    const char *short_hash = "{\"url\":\"https://fw.medicalopenworld.org/x.bin\",\"sha256\":\"abc\"}";
    TEST_ASSERT_EQUAL(OTA_CMD_BAD_SHA256, ota_validate_cmd(short_hash, strlen(short_hash), SUF, false, &job));
    const char *no_url = "{\"sha256\":\"" HASH "\"}";
    TEST_ASSERT_EQUAL(OTA_CMD_BAD_URL, ota_validate_cmd(no_url, strlen(no_url), SUF, false, &job));
    TEST_ASSERT_EQUAL(OTA_CMD_BAD_JSON, ota_validate_cmd("{", 1, SUF, false, &job));

    char long_url[400];
    strcpy(long_url, "{\"url\":\"https://fw.medicalopenworld.org/");
    memset(long_url + strlen(long_url), 'a', 260);
    strcpy(long_url + strlen("{\"url\":\"https://fw.medicalopenworld.org/") + 260, "\",\"sha256\":\"" HASH "\"}");
    TEST_ASSERT_EQUAL(OTA_CMD_BAD_URL, ota_validate_cmd(long_url, strlen(long_url), SUF, false, &job));
}
