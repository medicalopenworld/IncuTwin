#include "identity.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "storage.h"

static const char *TAG = "identity";

static char s_sn[24];
static char s_hwrev[12];
static char s_user[48];
static bool s_factory;
static bool s_has_creds;

void identity_init(void)
{
    s_factory = storage_get_str(STORAGE_NS_FACTORY, "sn", s_sn, sizeof(s_sn));
    storage_get_str(STORAGE_NS_FACTORY, "hwrev", s_hwrev, sizeof(s_hwrev));
    if (!s_factory) {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(s_sn, sizeof(s_sn), "ITW-%02X%02X%02X", mac[3], mac[4], mac[5]);
        ESP_LOGW(TAG, "sin NVS de fabrica: serie derivado de la MAC");
    }
    char pass[64];
    bool has_user = storage_get_str(STORAGE_NS_MQTT, "user", s_user, sizeof(s_user));
    bool has_pass = storage_get_str(STORAGE_NS_MQTT, "pass", pass, sizeof(pass));
    memset(pass, 0, sizeof(pass)); /* solo nos importa que exista */
    s_has_creds = has_user && has_pass;
    if (!s_has_creds) {
        ESP_LOGW(TAG, "panel sin serializar: faltan credenciales MQTT en NVS");
    }
    ESP_LOGI(TAG, "SN %s hwrev '%s' mqtt user '%s'", s_sn, s_hwrev, s_has_creds ? s_user : "-");
}

const char *identity_sn(void) { return s_sn; }
const char *identity_hwrev(void) { return s_hwrev; }
bool identity_has_factory_nvs(void) { return s_factory; }
bool identity_has_creds(void) { return s_has_creds; }
const char *identity_mqtt_user(void) { return s_has_creds ? s_user : ""; }

bool identity_mqtt_pass(char *out, size_t out_len)
{
    return storage_get_str(STORAGE_NS_MQTT, "pass", out, out_len);
}

bool identity_incubator_id(char out[TWIN_INCUBATOR_ID_LEN])
{
    return storage_get_str(STORAGE_NS_MQTT, "incubator_id", out, TWIN_INCUBATOR_ID_LEN);
}

bool identity_set_incubator_id(const char *id)
{
    if (!id || !id[0]) {
        return storage_erase_key(STORAGE_NS_MQTT, "incubator_id");
    }
    return storage_set_str(STORAGE_NS_MQTT, "incubator_id", id);
}

bool identity_prov_done(void)
{
    uint8_t d = 0;
    return storage_get_u8(STORAGE_NS_PROV, "done", &d) && d != 0;
}

void identity_prov_mark_done(void) { storage_set_u8(STORAGE_NS_PROV, "done", 1); }

bool identity_wifi_creds(char *ssid, size_t ssid_len, char *pass, size_t pass_len)
{
    if (!storage_get_str(STORAGE_NS_PROV, "ssid", ssid, ssid_len)) {
        return false;
    }
    if (!storage_get_str(STORAGE_NS_PROV, "pass", pass, pass_len)) {
        pass[0] = '\0'; /* red abierta */
    }
    return true;
}

bool identity_set_wifi_creds(const char *ssid, const char *pass)
{
    return storage_set_str(STORAGE_NS_PROV, "ssid", ssid) &&
           storage_set_str(STORAGE_NS_PROV, "pass", pass ? pass : "");
}
