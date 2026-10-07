#include "usage_stats.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "storage.h"

static const char *TAG = "usage";

#define SAVE_PERIOD_S 600 /* escribir NVS cada 10 min: poco desgaste */

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static usage_stats_t s;
static volatile bool s_hand;
static uint32_t s_last_save;

static void save(void)
{
    usage_stats_t c = usage_get();
    storage_set_u32(STORAGE_NS_USAGE, "on", c.on_s);
    storage_set_u32(STORAGE_NS_USAGE, "conn", c.conn_s);
    storage_set_u32(STORAGE_NS_USAGE, "hand", c.hand_s);
    storage_set_u32(STORAGE_NS_USAGE, "handn", c.hand_n);
}

void usage_init(void)
{
    storage_get_u32(STORAGE_NS_USAGE, "on", &s.on_s);
    storage_get_u32(STORAGE_NS_USAGE, "conn", &s.conn_s);
    storage_get_u32(STORAGE_NS_USAGE, "hand", &s.hand_s);
    storage_get_u32(STORAGE_NS_USAGE, "handn", &s.hand_n);
    s_last_save = s.on_s;
    ESP_LOGI(TAG, "on=%lus conn=%lus hand=%lus x%lu", (unsigned long)s.on_s,
             (unsigned long)s.conn_s, (unsigned long)s.hand_s, (unsigned long)s.hand_n);
}

void usage_tick_1s(bool incubator_connected)
{
    bool do_save = false;
    portENTER_CRITICAL(&s_mux);
    s.on_s++;
    if (incubator_connected) s.conn_s++;
    if (s_hand) s.hand_s++;
    if (s.on_s - s_last_save >= SAVE_PERIOD_S) {
        s_last_save = s.on_s;
        do_save = true;
    }
    portEXIT_CRITICAL(&s_mux);
    if (do_save) save();
}

void usage_hand_set(bool holding)
{
    portENTER_CRITICAL(&s_mux);
    if (holding && !s_hand) s.hand_n++;
    s_hand = holding;
    portEXIT_CRITICAL(&s_mux);
}

usage_stats_t usage_get(void)
{
    portENTER_CRITICAL(&s_mux);
    usage_stats_t c = s;
    portEXIT_CRITICAL(&s_mux);
    return c;
}
