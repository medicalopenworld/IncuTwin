#include "wifi_service.h"

#include <Arduino.h>
#include <WiFi.h>

#include "app/app_state.h"
#include "app/identity.h"
#include "config.h"

static void on_wifi_event(WiFiEvent_t event) {
    bool connected = (event == ARDUINO_EVENT_WIFI_STA_GOT_IP);
    bool disconnected = (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    if (!connected && !disconnected) return;

    state_lock();
    g_state.wifi_connected = connected;
    if (disconnected) g_state.cloud_connected = false;
    g_state_dirty = true;
    state_unlock();

    if (disconnected) WiFi.reconnect();
}

void wifi_service_start(void) {
    char ssid[33] = {0}, pass[65] = {0};
    if (!prov_get_wifi(ssid, sizeof(ssid), pass, sizeof(pass))) return;

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.onEvent(on_wifi_event);
    WiFi.begin(ssid, pass);
}
