#include "tb_client.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <MD5Builder.h>
#include <PubSubClient.h>
#include <Update.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app/app_state.h"
#include "app/identity.h"
#include "app/usage_stats.h"
#include "config.h"

#define OTA_CHUNK_SIZE 4096
#define MQTT_BUF_SIZE (OTA_CHUNK_SIZE + 512)

static WiFiClient s_net;
static PubSubClient s_mqtt(s_net);
static volatile tb_status_t s_status = TB_IDLE;
static char s_token[64] = {0};

/* provisioning response */
static volatile bool s_prov_answered = false;
static volatile bool s_prov_ok = false;

/* OTA state */
static struct {
    bool pending;            /* new fw announced                */
    bool active;             /* download in progress            */
    char title[32];
    char version[24];
    uint32_t size;
    char checksum[40];
    char algo[12];
    uint32_t received;
    int chunk;
    volatile bool chunk_ok;  /* current chunk arrived            */
    MD5Builder md5;
} s_ota;

/* ------------------------------------------------------------- publishing */

static void publish_json(const char *topic, JsonDocument &doc) {
    char buf[512];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    s_mqtt.publish(topic, (const uint8_t *)buf, n);
}

static void publish_fw_state(const char *state, const char *error = nullptr) {
    StaticJsonDocument<192> doc;
    doc["fw_state"] = state;
    if (error) doc["fw_error"] = error;
    publish_json("v1/devices/me/telemetry", doc);
}

static void publish_current_fw(void) {
    StaticJsonDocument<192> doc;
    doc["current_fw_title"] = FW_TITLE;
    doc["current_fw_version"] = FW_VERSION;
    publish_json("v1/devices/me/telemetry", doc);
}

static void publish_usage(void) {
    state_lock();
    bool inc_online = g_state.incubator_online && g_state.cloud_connected;
    state_unlock();

    StaticJsonDocument<320> doc;
    doc["on_hours"] = usage_on_s() / 3600.0;
    doc["connected_hours"] = usage_conn_s() / 3600.0;
    doc["hand_hours"] = usage_hand_s() / 3600.0;
    doc["hand_count"] = usage_hand_n();
    doc["incubator_online"] = inc_online;
    doc["rssi"] = WiFi.RSSI();
    doc["uptime_s"] = (uint32_t)(millis() / 1000);
    publish_json("v1/devices/me/telemetry", doc);
}

/* ------------------------------------------------------------------- OTA */

static void ota_check_attrs(JsonVariantConst attrs) {
    const char *title = attrs["fw_title"] | (const char *)nullptr;
    const char *ver = attrs["fw_version"] | (const char *)nullptr;
    if (!title || !ver) return;
    if (strcmp(title, FW_TITLE) != 0) return;
    if (strcmp(ver, FW_VERSION) == 0) return;

    strlcpy(s_ota.title, title, sizeof(s_ota.title));
    strlcpy(s_ota.version, ver, sizeof(s_ota.version));
    s_ota.size = attrs["fw_size"] | 0;
    strlcpy(s_ota.checksum, attrs["fw_checksum"] | "", sizeof(s_ota.checksum));
    strlcpy(s_ota.algo, attrs["fw_checksum_algorithm"] | "",
            sizeof(s_ota.algo));
    if (s_ota.size > 0) s_ota.pending = true;
}

static bool ota_request_chunk(int chunk) {
    char topic[48], payload[16];
    snprintf(topic, sizeof(topic), "v2/fw/request/1/chunk/%d", chunk);
    snprintf(payload, sizeof(payload), "%d", OTA_CHUNK_SIZE);
    s_ota.chunk = chunk;
    s_ota.chunk_ok = false;
    return s_mqtt.publish(topic, payload);
}

static void ota_run(void) {
    s_ota.active = true;
    s_status = TB_OTA_DOWNLOADING;
    publish_fw_state("DOWNLOADING");

    if (!Update.begin(s_ota.size)) {
        publish_fw_state("FAILED", "Update.begin failed");
        s_ota.active = s_ota.pending = false;
        s_status = TB_CONNECTED;
        return;
    }
    s_ota.md5.begin();
    s_ota.received = 0;

    int total_chunks = (s_ota.size + OTA_CHUNK_SIZE - 1) / OTA_CHUNK_SIZE;
    for (int c = 0; c < total_chunks; c++) {
        bool got = false;
        for (int retry = 0; retry < 3 && !got; retry++) {
            ota_request_chunk(c);
            uint32_t t0 = millis();
            while (millis() - t0 < 10000) {
                s_mqtt.loop();
                if (s_ota.chunk_ok) { got = true; break; }
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
        if (!got) {
            Update.abort();
            publish_fw_state("FAILED", "chunk timeout");
            s_ota.active = s_ota.pending = false;
            s_status = TB_CONNECTED;
            return;
        }
    }

    publish_fw_state("DOWNLOADED");

    if (strcasecmp(s_ota.algo, "MD5") == 0 && s_ota.checksum[0]) {
        s_ota.md5.calculate();
        if (strcasecmp(s_ota.md5.toString().c_str(), s_ota.checksum) != 0) {
            Update.abort();
            publish_fw_state("FAILED", "checksum mismatch");
            s_ota.active = s_ota.pending = false;
            s_status = TB_CONNECTED;
            return;
        }
    }
    publish_fw_state("VERIFIED");

    if (!Update.end(true)) {
        publish_fw_state("FAILED", "Update.end failed");
        s_ota.active = s_ota.pending = false;
        s_status = TB_CONNECTED;
        return;
    }
    publish_fw_state("UPDATING");
    s_mqtt.loop();
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP.restart();
}

/* --------------------------------------------------------------- callback */

static void mqtt_callback(char *topic, uint8_t *payload, unsigned int len) {
    /* provisioning response */
    if (strcmp(topic, "/provision/response") == 0) {
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, payload, len) == DeserializationError::Ok) {
            const char *status = doc["status"] | "";
            const char *cred = doc["credentialsValue"] | "";
            if (strcmp(status, "SUCCESS") == 0 && cred[0]) {
                strlcpy(s_token, cred, sizeof(s_token));
                s_prov_ok = true;
            }
        }
        s_prov_answered = true;
        return;
    }

    /* OTA chunk */
    if (strncmp(topic, "v2/fw/response/", 15) == 0) {
        if (s_ota.active) {
            Update.write(payload, len);
            s_ota.md5.add(payload, len);
            s_ota.received += len;
            s_ota.chunk_ok = true;
        }
        return;
    }

    /* shared attributes (pushed or requested) */
    if (strncmp(topic, "v1/devices/me/attributes", 24) == 0) {
        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, payload, len) != DeserializationError::Ok)
            return;
        if (doc.containsKey("shared"))
            ota_check_attrs(doc["shared"]);
        else
            ota_check_attrs(doc.as<JsonVariantConst>());
    }
}

/* ------------------------------------------------------------ provisioning */

static bool do_provision(void) {
    s_status = TB_PROVISIONING;
    s_mqtt.setServer(TB_HOST, TB_PORT);
    s_mqtt.setCallback(mqtt_callback);

    if (!s_mqtt.connect(identity_sn(), "provision", "")) {
        s_status = TB_PROVISION_FAILED;
        return false;
    }
    s_mqtt.subscribe("/provision/response");

    StaticJsonDocument<256> doc;
    doc["deviceName"] = identity_sn();
    doc["provisionDeviceKey"] = TB_PROVISION_KEY;
    doc["provisionDeviceSecret"] = TB_PROVISION_SECRET;
    doc["credentialsType"] = "ACCESS_TOKEN";
    s_prov_answered = false;
    s_prov_ok = false;
    publish_json("/provision/request", doc);

    uint32_t t0 = millis();
    while (millis() - t0 < 15000 && !s_prov_answered) {
        s_mqtt.loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    s_mqtt.disconnect();

    if (s_prov_ok) {
        prov_set_tb_token(s_token);
        return true;
    }
    s_status = TB_PROVISION_FAILED;
    return false;
}

/* -------------------------------------------------------------------- task */

static void tb_task(void *arg) {
    (void)arg;
    s_mqtt.setBufferSize(MQTT_BUF_SIZE);
    s_mqtt.setKeepAlive(TB_MQTT_KEEPALIVE_S);

    uint32_t last_telemetry = 0;

    while (true) {
        if (WiFi.status() != WL_CONNECTED) {
            s_status = TB_DISCONNECTED;
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        /* get credentials if we have none */
        if (s_token[0] == '\0' && !prov_get_tb_token(s_token, sizeof(s_token))) {
            if (!do_provision()) {
                vTaskDelay(pdMS_TO_TICKS(30000));
                continue;
            }
        }

        /* connect with the device token */
        if (!s_mqtt.connected()) {
            s_status = TB_DISCONNECTED;
            s_mqtt.setServer(TB_HOST, TB_PORT);
            s_mqtt.setCallback(mqtt_callback);
            if (!s_mqtt.connect(identity_sn(), s_token, "")) {
                vTaskDelay(pdMS_TO_TICKS(10000));
                continue;
            }
            s_status = TB_CONNECTED;
            s_mqtt.subscribe("v1/devices/me/attributes");
            s_mqtt.subscribe("v1/devices/me/attributes/response/+");
            s_mqtt.subscribe("v2/fw/response/+/chunk/+");
            publish_current_fw();
            publish_fw_state("UPDATED");
            /* ask for pending OTA info */
            StaticJsonDocument<128> req;
            req["sharedKeys"] =
                "fw_title,fw_version,fw_size,fw_checksum,fw_checksum_algorithm";
            publish_json("v1/devices/me/attributes/request/1", req);
        }

        s_mqtt.loop();

        if (s_ota.pending && !s_ota.active) ota_run();

        if (millis() - last_telemetry > TB_TELEMETRY_PERIOD_S * 1000UL) {
            last_telemetry = millis();
            publish_usage();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void tb_client_start(void) {
    xTaskCreatePinnedToCore(tb_task, "tb_mqtt", 10240, nullptr, 1, nullptr, 0);
}

tb_status_t tb_status(void) { return s_status; }

bool tb_has_token(void) {
    if (s_token[0]) return true;
    char tmp[64];
    return prov_get_tb_token(tmp, sizeof(tmp));
}
