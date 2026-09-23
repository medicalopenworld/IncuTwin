#include "identity.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>

static char s_sn[24] = {0};
static char s_hwrev[12] = {0};

void identity_init(void) {
    Preferences p;
    /* factory namespace is written by tools/factory_provision.py */
    if (p.begin("factory", true)) {
        p.getString("sn", s_sn, sizeof(s_sn));
        p.getString("hwrev", s_hwrev, sizeof(s_hwrev));
        p.end();
    }
    if (s_sn[0] == '\0') {
        /* fallback: derive from eFuse MAC (always unique) */
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(s_sn, sizeof(s_sn), "ITW-%02X%02X%02X", mac[3], mac[4],
                 mac[5]);
    }
}

const char *identity_sn(void) { return s_sn; }
const char *identity_hwrev(void) { return s_hwrev; }

/* ---------------------------------------------------------------- prov NVS */

static Preferences s_prov;

static void prov_open(bool ro) { s_prov.begin("prov", ro); }

bool prov_is_done(void) {
    prov_open(true);
    bool d = s_prov.getBool("done", false);
    s_prov.end();
    return d;
}

void prov_mark_done(void) {
    prov_open(false);
    s_prov.putBool("done", true);
    s_prov.end();
}

void prov_factory_reset(void) {
    prov_open(false);
    s_prov.clear();
    s_prov.end();
    ESP.restart();
}

bool prov_get_wifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len) {
    prov_open(true);
    size_t n = s_prov.getString("ssid", ssid, ssid_len);
    s_prov.getString("pass", pass, pass_len);
    s_prov.end();
    return n > 0;
}

void prov_set_wifi(const char *ssid, const char *pass) {
    prov_open(false);
    s_prov.putString("ssid", ssid);
    s_prov.putString("pass", pass);
    s_prov.end();
}

void prov_set_user(const char *name, const char *email, bool gdpr) {
    prov_open(false);
    s_prov.putString("name", name);
    s_prov.putString("email", email);
    s_prov.putBool("gdpr", gdpr);
    s_prov.end();
}

bool prov_get_name(char *out, size_t len) {
    prov_open(true);
    size_t n = s_prov.getString("name", out, len);
    s_prov.end();
    return n > 0;
}

bool prov_get_tb_token(char *out, size_t len) {
    prov_open(true);
    size_t n = s_prov.getString("token", out, len);
    s_prov.end();
    return n > 0;
}

void prov_set_tb_token(const char *token) {
    prov_open(false);
    s_prov.putString("token", token);
    s_prov.end();
}

void prov_clear_tb_token(void) {
    prov_open(false);
    s_prov.remove("token");
    s_prov.end();
}

const char *prov_pair_code(void) {
    static char code[8] = {0};
    if (code[0]) return code;
    prov_open(false);
    if (s_prov.getString("pcode", code, sizeof(code)) == 0 || !code[0]) {
        snprintf(code, sizeof(code), "%06u",
                 (unsigned)(esp_random() % 1000000));
        s_prov.putString("pcode", code);
    }
    s_prov.end();
    return code;
}
