#include "app_events.h"

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "events";

ESP_EVENT_DEFINE_BASE(TWIN_EVENTS);

static esp_event_loop_handle_t s_loop;

esp_err_t app_events_init(void)
{
    if (s_loop) {
        return ESP_OK;
    }
    esp_event_loop_args_t args = {
        .queue_size = 16,
        .task_name = "incutwin_evt",
        .task_priority = 5,
        .task_stack_size = 6144,
        .task_core_id = 0,
    };
    ESP_RETURN_ON_ERROR(esp_event_loop_create(&args, &s_loop), TAG, "loop");
    return ESP_OK;
}

esp_err_t app_events_post(app_event_id_t id, const void *data, size_t size)
{
    if (!s_loop) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_event_post_to(s_loop, TWIN_EVENTS, id, data, size, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "post %d: %s", (int)id, esp_err_to_name(err));
    }
    return err;
}

esp_err_t app_events_subscribe(app_event_id_t id, esp_event_handler_t handler, void *arg)
{
    if (!s_loop) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_event_handler_register_with(s_loop, TWIN_EVENTS, id, handler, arg);
}
