#include "sim_server.h"

#ifdef SIM_MODE

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app/app_state.h"

static WebServer *s_http = nullptr;

/* ---------------------------------------------------------------- escenarios */

typedef struct {
    const char *id;
    bool linked;   /* node_seen  */
    bool online;   /* incubator_online */
    thermo_state_t thermo;
    bool photo;
    uint16_t hr;
    bool awake;
    bool baby;
} sim_scenario_t;

static const sim_scenario_t SCENARIOS[] = {
    {"sleep",    true,  true,  THERMO_STABLE,  false, 120, false, true},
    {"awake",    true,  true,  THERMO_STABLE,  false, 140, true,  true},
    {"heating",  true,  true,  THERMO_HEATING, false, 130, false, true},
    {"alarm",    true,  true,  THERMO_ALARM,   false, 180, true,  true},
    {"photo",    true,  true,  THERMO_STABLE,  true,  130, false, true},
    {"empty",    true,  true,  THERMO_STABLE,  false, 0,   false, false},
    {"off",      true,  false, THERMO_OFF,     false, 0,   false, true},
    {"unlinked", false, false, THERMO_OFF,     false, 0,   false, true},
};
static const size_t N_SCENARIOS = sizeof(SCENARIOS) / sizeof(SCENARIOS[0]);

/* Aplica un escenario. Debe llamarse SIN el lock cogido. */
static bool apply_scenario(const char *id) {
    for (size_t i = 0; i < N_SCENARIOS; i++) {
        if (strcmp(SCENARIOS[i].id, id) != 0) continue;
        const sim_scenario_t &sc = SCENARIOS[i];
        state_lock();
        g_state.node_seen = sc.linked;
        g_state.incubator_online = sc.online;
        g_state.thermo = sc.thermo;
        g_state.phototherapy = sc.photo;
        g_state.heart_rate = sc.hr;
        g_state.awake = sc.awake;
        g_state.baby_present = sc.baby;
        g_state.last_update_ms = millis();
        g_state_dirty = true;
        state_unlock();
        return true;
    }
    return false;
}

/* ------------------------------------------------------------- campo a campo */

/* Aplica los campos presentes en el JSON. Llamar SIN el lock cogido. */
static void apply_fields(JsonObjectConst obj) {
    state_lock();
    for (JsonPairConst kv : obj) {
        const char *key = kv.key().c_str();
        JsonVariantConst v = kv.value();
        if (!strcmp(key, "online")) {
            g_state.incubator_online = v.as<bool>();
        } else if (!strcmp(key, "linked")) {
            g_state.node_seen = v.as<bool>();
        } else if (!strcmp(key, "thermo")) {
            g_state.thermo = thermo_from_str(v.as<const char *>());
        } else if (!strcmp(key, "photo")) {
            g_state.phototherapy = v.as<bool>();
        } else if (!strcmp(key, "hr")) {
            int hr = v.as<int>();
            g_state.heart_rate = (uint16_t)constrain(hr, 0, 300);
        } else if (!strcmp(key, "skin")) {
            int t = v.as<int>();
            if (t >= 0 && t < 6) g_state.skin_tone = (uint8_t)t;
        } else if (!strcmp(key, "awake")) {
            g_state.awake = v.as<bool>();
        } else if (!strcmp(key, "baby")) {
            g_state.baby_present = v.as<bool>();
        }
        /* "scenario" ya se procesó en el handler; claves desconocidas
         * se ignoran, como hace firebase_stream. */
    }
    g_state.last_update_ms = millis();
    g_state_dirty = true;
    state_unlock();
}

/* ---------------------------------------------------------------- handlers */

static const char *thermo_str(thermo_state_t t) {
    switch (t) {
        case THERMO_HEATING: return "heating";
        case THERMO_STABLE: return "stable";
        case THERMO_ALARM: return "alarm";
        default: return "off";
    }
}

static void handle_state_get(void) {
    StaticJsonDocument<256> doc;
    state_lock();
    doc["online"] = g_state.incubator_online;
    doc["linked"] = g_state.node_seen;
    doc["thermo"] = thermo_str(g_state.thermo);
    doc["photo"] = g_state.phototherapy;
    doc["hr"] = g_state.heart_rate;
    doc["skin"] = g_state.skin_tone;
    doc["awake"] = g_state.awake;
    doc["baby"] = g_state.baby_present;
    state_unlock();
    String out;
    serializeJson(doc, out);
    s_http->send(200, "application/json", out);
}

static void handle_state_post(void) {
    String body = s_http->arg("plain");
    if (body.length() == 0 || body.length() > 768) {
        s_http->send(400, "text/plain", "bad body size");
        return;
    }
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok ||
        !doc.is<JsonObjectConst>()) {
        s_http->send(400, "text/plain", "bad json");
        return;
    }
    JsonObjectConst obj = doc.as<JsonObjectConst>();

    /* primero el escenario (si viene), luego los campos lo refinan */
    const char *scen = obj["scenario"] | (const char *)nullptr;
    if (scen && !apply_scenario(scen)) {
        s_http->send(400, "text/plain", "unknown scenario");
        return;
    }
    apply_fields(obj);
    handle_state_get(); /* responde con el estado resultante */
}

static void handle_root(void) {
    s_http->send(200, "text/plain", "IncuTwin SIM_MODE ok");
}

/* --------------------------------------------------------------------- task */

static void sim_task(void *arg) {
    (void)arg;
    uint32_t last_touch = 0;
    while (true) {
        if (s_http) s_http->handleClient();

        /* Refresca last_update_ms cada segundo para que el watchdog
         * DATA_STALE_S no marque la incubadora como desconectada. */
        if (millis() - last_touch >= 1000) {
            last_touch = millis();
            state_lock();
            g_state.last_update_ms = millis();
            state_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/* ---------------------------------------------------------------------- API */

void sim_server_start(void) {
    /* Estado inicial sano: como si hubiera IncuNest vinculada y en línea. */
    state_lock();
    g_state.cloud_connected = true;
    g_state.node_seen = true;
    g_state.incubator_online = true;
    g_state.thermo = THERMO_STABLE;
    g_state.heart_rate = 120;
    g_state.awake = false;
    g_state.baby_present = true;
    g_state.last_update_ms = millis();
    g_state_dirty = true;
    state_unlock();

    s_http = new WebServer(80);
    s_http->on("/", handle_root);
    s_http->on("/state", HTTP_GET, handle_state_get);
    s_http->on("/state", HTTP_POST, handle_state_post);
    s_http->begin();

    xTaskCreatePinnedToCore(sim_task, "sim_http", 8192, nullptr, 1, nullptr, 0);
    Serial.println("[SIM] servidor de simulacion en puerto 80");
}

#else /* !SIM_MODE */

void sim_server_start(void) {}

#endif
