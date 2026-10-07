#include "commands.h"

#include <string.h>

#include "app_events.h"
#include "commands_parse.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "mqtt_link.h"
#include "pairing.h"
#include "settings.h"
#include "sound.h"
#include "twin_model.h"

static const char *TAG = "cmd";

#define REBOOT_UNICAST_MS   500
#define REBOOT_FLEET_MAX_MS 60000

static esp_timer_handle_t s_reboot_timer;

/* Gancho de la OTA (tarea 5): hasta entonces solo se registra. */
void __attribute__((weak)) ota_update_request(const char *payload, size_t len, bool fleet)
{
    (void)payload; (void)len;
    ESP_LOGW(TAG, "cmd/ota recibido (%s): OTA no disponible en esta build", fleet ? "flota" : "unicast");
}

static void reboot_cb(void *arg)
{
    (void)arg;
    ESP_LOGW(TAG, "reiniciando por comando");
    esp_restart();
}

static void cmd_reboot(bool fleet)
{
    uint32_t ms = fleet ? esp_random() % (REBOOT_FLEET_MAX_MS + 1) : REBOOT_UNICAST_MS;
    ESP_LOGI(TAG, "reboot en %lu ms", (unsigned long)ms);
    esp_timer_stop(s_reboot_timer);
    esp_timer_start_once(s_reboot_timer, (uint64_t)ms * 1000ULL);
}

static void cmd_test_melody(const char *payload, size_t len)
{
    char m[16];
    cmd_parse_melody(payload, len, m);
    sound_melody_t sel = SOUND_BABY;
    if (!strcmp(m, "boot")) sel = SOUND_BOOT;
    else if (!strcmp(m, "parents")) sel = SOUND_PARENTS;
    else if (!strcmp(m, "test")) sel = SOUND_TEST;
    sound_request(sel);
}

static void cmd_brightness(const char *payload, size_t len)
{
    int v;
    if (!cmd_parse_brightness(payload, len, &v)) {
        ESP_LOGW(TAG, "brightness: payload invalido");
        return;
    }
    settings_set_brightness((uint8_t)v);
    ESP_LOGI(TAG, "brightness %d%%", v);
}

static void on_msg(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    const app_evt_mqtt_msg_t *m = data;

    if (cmd_is_state_topic(m->topic, NULL)) {
        twin_model_ingest(m->payload, m->len);
        return;
    }
    char name[CMD_NAME_MAX];
    cmd_topic_kind_t kind = cmd_parse_topic(m->topic, mqtt_link_client_id(), name);
    if (kind == CMD_TOPIC_NONE) {
        ESP_LOGW(TAG, "topic ignorado: %s", m->topic);
        return;
    }
    bool fleet = kind == CMD_TOPIC_FLEET;
    ESP_LOGI(TAG, "cmd %s (%s) %u B", name, fleet ? "flota" : "unicast", m->len);

    if (!strcmp(name, "pair")) {
        if (fleet) ESP_LOGW(TAG, "pair por flota ignorado");
        else pairing_handle(m->payload, m->len);
    } else if (!strcmp(name, "ota")) {
        ota_update_request(m->payload, m->len, fleet);
    } else if (!strcmp(name, "reboot")) {
        cmd_reboot(fleet);
    } else if (!strcmp(name, "test_melody")) {
        cmd_test_melody(m->payload, m->len);
    } else if (!strcmp(name, "brightness")) {
        cmd_brightness(m->payload, m->len);
    } else {
        ESP_LOGW(TAG, "comando desconocido: %s", name);
    }
}

void commands_start(void)
{
    const esp_timer_create_args_t t = { .callback = reboot_cb, .name = "cmd_reboot" };
    ESP_ERROR_CHECK(esp_timer_create(&t, &s_reboot_timer));
    app_events_subscribe(TWIN_EVT_MQTT_MSG, on_msg, NULL);
    pairing_init();
}
