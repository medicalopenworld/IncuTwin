#include "firebase_stream.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app/app_state.h"
#include "app/identity.h"
#include "config.h"

/* ------------------------------------------------------------------------- */

static WiFiClientSecure s_client;
static String s_host = FIREBASE_HOST;
static String s_path; /* set on start */

static void set_cloud(bool ok) {
    state_lock();
    if (g_state.cloud_connected != ok) {
        g_state.cloud_connected = ok;
        g_state_dirty = true;
    }
    state_unlock();
}

/* ---- apply one SSE event to g_state -------------------------------------- */

static void apply_field(const char *key, JsonVariantConst v) {
    if (!strcmp(key, "online")) {
        g_state.incubator_online = v.as<bool>();
    } else if (!strcmp(key, "thermo")) {
        g_state.thermo = thermo_from_str(v.as<const char *>());
    } else if (!strcmp(key, "photo")) {
        g_state.phototherapy = v.as<bool>();
    } else if (!strcmp(key, "hr")) {
        g_state.heart_rate = v.as<uint16_t>();
    } else if (!strcmp(key, "skin")) {
        int t = v.as<int>();
        if (t >= 0 && t < 6) g_state.skin_tone = (uint8_t)t;
    } else if (!strcmp(key, "awake")) {
        g_state.awake = v.as<bool>();
    } else if (!strcmp(key, "baby")) {
        g_state.baby_present = v.as<bool>();
    }
}

static void apply_event(const char *event, const String &data) {
    if (!strcmp(event, "keep-alive")) {
        /* SSE: sin eventos = sin cambios; el keep-alive prueba que el
         * estado sigue vigente, asi que cuenta como dato fresco */
        state_lock();
        g_state.last_update_ms = millis();
        state_unlock();
        return;
    }
    if (strcmp(event, "put") != 0 && strcmp(event, "patch") != 0) return;

    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, data) != DeserializationError::Ok) return;

    const char *path = doc["path"] | "/";
    JsonVariantConst payload = doc["data"];

    state_lock();
    if (!strcmp(path, "/")) {
        if (payload.is<JsonObjectConst>()) {
            for (JsonPairConst kv : payload.as<JsonObjectConst>())
                apply_field(kv.key().c_str(), kv.value());
            g_state.node_seen = true;
            g_state.last_update_ms = millis();
        } else if (payload.isNull()) {
            /* the state node does not exist: no IncuNest linked yet */
            g_state.node_seen = false;
            g_state.incubator_online = false;
        }
    } else {
        apply_field(path + 1, payload); /* "/thermo" -> "thermo" */
        g_state.node_seen = true;
        g_state.last_update_ms = millis();
    }
    g_state_dirty = true;
    state_unlock();
}

/* ---- HTTP / SSE plumbing -------------------------------------------------- */

static bool read_line(WiFiClient &c, String &line, uint32_t timeout_ms) {
    line = "";
    uint32_t t0 = millis();
    while (millis() - t0 < timeout_ms) {
        if (!c.connected() && !c.available()) return false;
        int ch = c.read();
        if (ch < 0) {
            delay(2);
            continue;
        }
        if (ch == '\n') {
            if (line.endsWith("\r")) line.remove(line.length() - 1);
            return true;
        }
        line += (char)ch;
        t0 = millis(); /* got a byte: reset timeout */
    }
    return false;
}

/* Reads the SSE stream. Returns only on error/disconnect. */
static void pump_stream(bool chunked) {
    String event, data, line;
    long chunk_left = chunked ? 0 : LONG_MAX;

    auto next_line = [&](String &out) -> bool {
        if (!chunked) return read_line(s_client, out, 65000);
        /* chunked: honour chunk boundaries while assembling text lines */
        out = "";
        while (true) {
            if (chunk_left <= 0) {
                String szline;
                if (!read_line(s_client, szline, 65000)) return false;
                if (szline.length() == 0) continue; /* CRLF after chunk */
                chunk_left = strtol(szline.c_str(), nullptr, 16);
                if (chunk_left == 0) return false; /* end of stream */
                continue;
            }
            int ch = s_client.read();
            if (ch < 0) {
                if (!s_client.connected()) return false;
                delay(2);
                continue;
            }
            chunk_left--;
            if (ch == '\n') {
                if (out.endsWith("\r")) out.remove(out.length() - 1);
                return true;
            }
            out += (char)ch;
        }
    };

    while (s_client.connected()) {
        if (!next_line(line)) return;

        if (line.length() == 0) { /* dispatch */
            if (event.length() && data.length()) {
                if (event == "auth_revoked" || event == "cancel") return;
                apply_event(event.c_str(), data);
                set_cloud(true);
            }
            event = "";
            data = "";
        } else if (line.startsWith("event:")) {
            event = line.substring(6);
            event.trim();
        } else if (line.startsWith("data:")) {
            data = line.substring(5);
            data.trim();
        }
    }
}

/* Opens the SSE connection (following one redirect if needed).
 * Returns false on failure. */
static bool open_stream(void) {
    String host = s_host;
    String path = s_path;

    for (int hop = 0; hop < 3; hop++) {
        s_client.stop();
        s_client.setInsecure(); /* TODO: pin Firebase root CA in production */
        s_client.setTimeout(15);
        if (!s_client.connect(host.c_str(), 443)) return false;

        String url = path + ".json";
        if (strlen(FIREBASE_AUTH) > 0) url += String("?auth=") + FIREBASE_AUTH;

        s_client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                       "Host: " + host + "\r\n" +
                       "Accept: text/event-stream\r\n" +
                       "Connection: keep-alive\r\n\r\n");

        String line;
        if (!read_line(s_client, line, 10000)) return false;
        int status = 0;
        if (line.startsWith("HTTP/")) status = line.substring(9, 12).toInt();

        String location;
        bool chunked = false;
        while (read_line(s_client, line, 10000)) {
            if (line.length() == 0) break; /* end of headers */
            String low = line;
            low.toLowerCase();
            if (low.startsWith("location:")) {
                location = line.substring(9);
                location.trim();
            } else if (low.startsWith("transfer-encoding:") &&
                       low.indexOf("chunked") >= 0) {
                chunked = true;
            }
        }

        if (status == 200) {
            pump_stream(chunked);
            return true; /* stream ended -> reconnect from caller */
        }
        if ((status == 307 || status == 302) && location.length()) {
            /* https://new-host/new-path.json?... */
            int hs = location.indexOf("://");
            if (hs < 0) return false;
            int he = location.indexOf('/', hs + 3);
            host = location.substring(hs + 3, he);
            path = location.substring(he);
            int q = path.indexOf(".json");
            if (q >= 0) path = path.substring(0, q);
            continue;
        }
        return false;
    }
    return false;
}

/* ---- task ------------------------------------------------------------------ */

static void stream_task(void *arg) {
    (void)arg;
    uint32_t backoff = 1000;

    while (true) {
        if (WiFi.status() != WL_CONNECTED) {
            set_cloud(false);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        bool had_stream = open_stream();
        s_client.stop();

        if (had_stream) {
            /* Firebase recicla streams sanos: reconectar sin marcar la
             * nube caida, o el bebe parpadea en cada reciclo */
            backoff = 1000;
        } else {
            set_cloud(false);
            backoff = min<uint32_t>(backoff * 2, 30000);
        }
        vTaskDelay(pdMS_TO_TICKS(backoff));
    }
}

void firebase_stream_start(void) {
    char path[96];
    snprintf(path, sizeof(path), FIREBASE_STATE_PATH_FMT, identity_sn());
    s_path = path;
    xTaskCreatePinnedToCore(stream_task, "fb_sse", 8192, nullptr, 1, nullptr, 0);
}
