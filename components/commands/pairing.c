#include "pairing.h"

#include <string.h>

#include "app_events.h"
#include "commands_parse.h"
#include "esp_log.h"
#include "identity.h"
#include "mqtt_link.h"

static const char *TAG = "pairing";

static void announce(const char *id)
{
    app_evt_pairing_t ev = { 0 };
    strncpy(ev.incubator_id, id, TWIN_INCUBATOR_ID_LEN - 1);
    app_events_post(TWIN_EVT_PAIRING_CHANGED, &ev, sizeof(ev)); /* twin_model, mqtt_link, status */
}

static void on_sub_rejected(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    const app_evt_pairing_t *ev = data;
    char current[TWIN_INCUBATOR_ID_LEN] = "";
    identity_incubator_id(current);
    if (!current[0] || strcmp(current, ev->incubator_id) != 0) return;
    identity_set_incubator_id(NULL);
    ESP_LOGW(TAG, "desemparejado de %s (el broker rechazo la suscripcion a su estado)", current);
    announce("");
}

void pairing_init(void)
{
    app_events_subscribe(TWIN_EVT_STATE_SUB_REJECTED, on_sub_rejected, NULL);
}

void pairing_handle(const char *payload, size_t len)
{
    char id[TWIN_INCUBATOR_ID_LEN];
    char current[TWIN_INCUBATOR_ID_LEN] = "";
    identity_incubator_id(current);

    switch (cmd_parse_pair(payload, len, id)) {
    case PAIR_SET:
        if (strcmp(id, current) == 0) {
            mqtt_link_set_incubator(id); /* mismo id: solo asegurar la suscripcion */
            return;
        }
        identity_set_incubator_id(id);
        ESP_LOGI(TAG, "emparejado con %s%s%s", id, current[0] ? " (antes " : "", current[0] ? current : "");
        announce(id);
        break;
    case PAIR_UNPAIR:
        if (!current[0]) return;
        identity_set_incubator_id(NULL);
        ESP_LOGI(TAG, "desemparejado de %s", current);
        announce("");
        break;
    default:
        ESP_LOGW(TAG, "cmd/pair invalido: %.*s", (int)(len > 80 ? 80 : len), payload);
        break;
    }
}
