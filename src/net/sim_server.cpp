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

/* ----------------------------------------------------------------- handlers */

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
    s_http->begin();

    xTaskCreatePinnedToCore(sim_task, "sim_http", 8192, nullptr, 1, nullptr, 0);
    Serial.println("[SIM] servidor de simulacion en puerto 80");
}

#else /* !SIM_MODE */

void sim_server_start(void) {}

#endif
