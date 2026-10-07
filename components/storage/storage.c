#include "storage.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "storage";

esp_err_t storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "nvs incompatible (%s): borrando", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static bool open_ns(const char *ns, nvs_open_mode_t mode, nvs_handle_t *h)
{
    esp_err_t err = nvs_open(ns, mode, h);
    if (err != ESP_OK) {
        if (!(mode == NVS_READONLY && err == ESP_ERR_NVS_NOT_FOUND)) {
            ESP_LOGW(TAG, "open %s: %s", ns, esp_err_to_name(err));
        }
        return false;
    }
    return true;
}

bool storage_get_str(const char *ns, const char *key, char *out, size_t out_len)
{
    nvs_handle_t h;
    if (!open_ns(ns, NVS_READONLY, &h)) {
        return false;
    }
    size_t len = out_len;
    esp_err_t err = nvs_get_str(h, key, out, &len);
    nvs_close(h);
    if (err != ESP_OK) {
        if (out_len) {
            out[0] = '\0';
        }
        return false;
    }
    return out[0] != '\0';
}

#define GET_IMPL(fn, type, nvsfn)                                  \
    bool fn(const char *ns, const char *key, type *out)            \
    {                                                              \
        nvs_handle_t h;                                            \
        if (!open_ns(ns, NVS_READONLY, &h)) return false;          \
        esp_err_t err = nvsfn(h, key, out);                        \
        nvs_close(h);                                              \
        return err == ESP_OK;                                      \
    }

GET_IMPL(storage_get_u8, uint8_t, nvs_get_u8)
GET_IMPL(storage_get_u32, uint32_t, nvs_get_u32)
GET_IMPL(storage_get_i32, int32_t, nvs_get_i32)

static bool commit_close(nvs_handle_t h, esp_err_t err, const char *ns, const char *key)
{
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set %s/%s: %s", ns, key, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool storage_set_str(const char *ns, const char *key, const char *val)
{
    nvs_handle_t h;
    if (!open_ns(ns, NVS_READWRITE, &h)) {
        return false;
    }
    return commit_close(h, nvs_set_str(h, key, val), ns, key);
}

#define SET_IMPL(fn, type, nvsfn)                                  \
    bool fn(const char *ns, const char *key, type val)             \
    {                                                              \
        nvs_handle_t h;                                            \
        if (!open_ns(ns, NVS_READWRITE, &h)) return false;         \
        return commit_close(h, nvsfn(h, key, val), ns, key);       \
    }

SET_IMPL(storage_set_u8, uint8_t, nvs_set_u8)
SET_IMPL(storage_set_u32, uint32_t, nvs_set_u32)
SET_IMPL(storage_set_i32, int32_t, nvs_set_i32)

bool storage_erase_key(const char *ns, const char *key)
{
    nvs_handle_t h;
    if (!open_ns(ns, NVS_READWRITE, &h)) {
        return false;
    }
    esp_err_t err = nvs_erase_key(h, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    return commit_close(h, err, ns, key);
}

bool storage_erase_ns(const char *ns)
{
    nvs_handle_t h;
    if (!open_ns(ns, NVS_READWRITE, &h)) {
        return false;
    }
    return commit_close(h, nvs_erase_all(h), ns, "*");
}

void storage_factory_reset(void)
{
    ESP_LOGW(TAG, "factory reset: borrando prov, settings y twin");
    storage_erase_ns(STORAGE_NS_PROV);
    storage_erase_ns(STORAGE_NS_SETTINGS);
    storage_erase_ns(STORAGE_NS_TWIN);
    vTaskDelay(pdMS_TO_TICKS(200)); /* que salga el log */
    esp_restart();
}
