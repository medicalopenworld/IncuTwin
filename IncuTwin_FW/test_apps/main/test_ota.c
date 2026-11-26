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

/* Firmware objetivo retenido (cmd/fw, spec ota-update «Firmware objetivo») */
TEST_CASE("ota: comparar versiones", "[ota]")
{
    TEST_ASSERT_TRUE(ota_version_cmp("2.0.1", "2.0.0") > 0);
    TEST_ASSERT_TRUE(ota_version_cmp("2.10.0", "2.9.9") > 0);
    TEST_ASSERT_TRUE(ota_version_cmp("2.0.0", "2.0.0") == 0);
    TEST_ASSERT_TRUE(ota_version_cmp("1.9", "2.0.0") < 0);
    TEST_ASSERT_TRUE(ota_version_cmp("2.0", "2.0.0") == 0);
    TEST_ASSERT_TRUE(ota_version_cmp("2.0.1-dev", "2.0.1") == 0); /* el sufijo no cuenta */
}

TEST_CASE("ota: objetivo valido y cuando se quiere", "[ota]")
{

    const char *ok = "{\"version\":\"2.1.0\",\"url\":\"https://fw.medicalopenworld.org/incutwin/incutwin-2.1.0.bin\","
                     "\"sha256\":\"bf3ef643a2fa7544910d9fc564348d689e2db1d40273b2e3654bb5ef947d7e87\"}";
    ota_target_t t;
    TEST_ASSERT_EQUAL(OTA_CMD_OK, ota_validate_target(ok, strlen(ok), SUF, false, &t));
    TEST_ASSERT_EQUAL_STRING("2.1.0", t.version);
    TEST_ASSERT_TRUE(ota_target_wanted("2.0.0", &t, NULL));   /* por detras: se actualiza */
    TEST_ASSERT_FALSE(ota_target_wanted("2.1.0", &t, NULL));  /* ya la tiene */
    TEST_ASSERT_FALSE(ota_target_wanted("2.2.0", &t, NULL));  /* nunca baja */
    TEST_ASSERT_FALSE(ota_target_wanted("2.0.0", &t, t.job.sha256)); /* ese binario ya fallo */
    const char *no_ver = "{\"url\":\"https://fw.medicalopenworld.org/x.bin\","
                         "\"sha256\":\"bf3ef643a2fa7544910d9fc564348d689e2db1d40273b2e3654bb5ef947d7e87\"}";
    TEST_ASSERT_EQUAL(OTA_CMD_BAD_VERSION, ota_validate_target(no_ver, strlen(no_ver), SUF, false, &t));
}
