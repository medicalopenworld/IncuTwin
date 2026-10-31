#include "ota_update.h"

#include <string.h>

#include "app_events.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ota_validate.h"
#include "psa/crypto.h" /* Mbed TLS 4: el hash va por la API PSA */
#include "sdkconfig.h"

static const char *TAG = "ota";

#define RX_BUF_SIZE   4096
#define HTTP_TIMEOUT  10000
#define MAX_REDIRECTS 3

#ifdef CONFIG_INCUTWIN_OTA_ALLOW_ANY_HOST
#define ALLOW_ANY_HOST true
#else
#define ALLOW_ANY_HOST false
#endif

static SemaphoreHandle_t s_mutex;
static ota_job_t s_pending;      /* trabajo en espera (retardo de flota) */
static bool s_has_pending;
static bool s_running;
static esp_timer_handle_t s_delay_timer;
static esp_timer_handle_t s_validate_timer;
static bool s_pending_verify;

/* ------------------------------------------------------------- rollback */

static void validate_timeout_cb(void *arg)
{
    (void)arg;
    if (!s_pending_verify) return;
    ESP_LOGE(TAG, "%d s sin conectar al broker: firmware marcado invalido, rollback",
             CONFIG_INCUTWIN_OTA_VALIDATE_TIMEOUT_S);
    esp_ota_mark_app_invalid_rollback_and_reboot();
}

static void on_mqtt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    if (!s_pending_verify || !((const app_evt_mqtt_t *)data)->connected) return;
    s_pending_verify = false;
    esp_timer_stop(s_validate_timer);
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGI(TAG, "app marked valid (%s)", esp_err_to_name(err));
}

/* ------------------------------------------------------------- download */

static void ota_fail(esp_ota_handle_t h, esp_http_client_handle_t http, const char *why)
{
    ESP_LOGE(TAG, "OTA abortada: %s", why);
    if (h) esp_ota_abort(h);
    if (http) {
        esp_http_client_close(http);
        esp_http_client_cleanup(http);
    }
}

static void ota_task(void *arg)
{
    ota_job_t job = *(ota_job_t *)arg;
    esp_ota_handle_t handle = 0;
    esp_http_client_handle_t http = NULL;
    uint8_t *buf = malloc(RX_BUF_SIZE);
    psa_hash_operation_t sha = PSA_HASH_OPERATION_INIT;
    bool sha_active = false;

    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    if (!target || !buf) {
        ota_fail(0, NULL, "sin particion destino o sin memoria");
        goto done;
    }
    if (psa_crypto_init() != PSA_SUCCESS || psa_hash_setup(&sha, PSA_ALG_SHA_256) != PSA_SUCCESS) {
        ota_fail(0, NULL, "psa sha256");
        goto done;
    }
    sha_active = true;
    ESP_LOGI(TAG, "descargando %s -> %s", job.url, target->label);

    esp_http_client_config_t cfg = {
        .url = job.url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = HTTP_TIMEOUT,
        .max_redirection_count = MAX_REDIRECTS,
        .keep_alive_enable = true,
        .buffer_size = RX_BUF_SIZE,
    };
    http = esp_http_client_init(&cfg);
    if (!http || esp_http_client_open(http, 0) != ESP_OK) {
        ota_fail(0, http, "no se pudo abrir la conexion HTTPS");
        goto done;
    }
    int64_t total = esp_http_client_fetch_headers(http);
    int status = esp_http_client_get_status_code(http);
    if (status != 200) {
        ota_fail(0, http, "HTTP distinto de 200");
        goto done;
    }
    if (total > (int64_t)target->size) {
        ota_fail(0, http, "el binario no cabe en el slot");
        goto done;
    }
    if (esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &handle) != ESP_OK) {
        ota_fail(0, http, "esp_ota_begin");
        goto done;
    }

    int64_t received = 0;
    int next_pct = 10;
    while (true) {
        int n = esp_http_client_read(http, (char *)buf, RX_BUF_SIZE);
        if (n < 0) {
            ota_fail(handle, http, "error de red durante la descarga");
            goto done;
        }
        if (n == 0) {
            if (esp_http_client_is_complete_data_received(http) || total <= 0) break;
            ota_fail(handle, http, "conexion cortada antes de completar");
            goto done;
        }
        if (esp_ota_write(handle, buf, n) != ESP_OK) {
            ota_fail(handle, http, "esp_ota_write");
            goto done;
        }
        psa_hash_update(&sha, buf, (size_t)n);
        received += n;
        if (received > (int64_t)target->size) {
            ota_fail(handle, http, "mas datos que el slot");
            goto done;
        }
        if (total > 0 && received * 100 / total >= next_pct) {
            ESP_LOGI(TAG, "%d %% (%lld / %lld B)", next_pct, (long long)received, (long long)total);
            next_pct += 10;
        }
    }
    esp_http_client_close(http);
    esp_http_client_cleanup(http);
    http = NULL;

    uint8_t digest[32];
    size_t digest_len = 0;
    psa_status_t ps = psa_hash_finish(&sha, digest, sizeof(digest), &digest_len);
    sha_active = false;
    if (ps != PSA_SUCCESS || digest_len != 32 || memcmp(digest, job.sha256, 32) != 0) {
        ota_fail(handle, NULL, "sha256 mismatch");
        goto done;
    }
    ESP_LOGI(TAG, "sha256 correcto (%lld B)", (long long)received);
    esp_err_t err = esp_ota_end(handle);
    handle = 0;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end: %s (imagen invalida)", esp_err_to_name(err));
        goto done;
    }
    if (esp_ota_set_boot_partition(target) != ESP_OK) {
        ESP_LOGE(TAG, "no se pudo fijar la particion de arranque");
        goto done;
    }
    ESP_LOGW(TAG, "firmware instalado en %s: reiniciando en 1 s", target->label);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

done:
    if (sha_active) psa_hash_abort(&sha);
    free(buf);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_running = false;
    xSemaphoreGive(s_mutex);
    vTaskDelete(NULL);
}

static void start_job_locked(const ota_job_t *job)
{
    static ota_job_t job_copy; /* la tarea la copia al arrancar */
    job_copy = *job;
    s_running = true;
    s_has_pending = false;
    if (xTaskCreatePinnedToCore(ota_task, "ota_task", 8192, &job_copy, 3, NULL, 0) != pdPASS) {
        s_running = false;
        ESP_LOGE(TAG, "no se pudo crear la tarea OTA");
    }
}

static void delay_cb(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_has_pending && !s_running) {
        ESP_LOGI(TAG, "retardo de flota vencido");
        start_job_locked(&s_pending);
    }
    xSemaphoreGive(s_mutex);
}

/* ------------------------------------------------------------------ API */

void ota_update_request(const char *payload, size_t len, bool fleet)
{
    ota_job_t job;
    ota_cmd_result_t r = ota_validate_cmd(payload, len, CONFIG_INCUTWIN_OTA_HOST_SUFFIX, ALLOW_ANY_HOST, &job);
    if (r != OTA_CMD_OK) {
        ESP_LOGW(TAG, "cmd/ota rechazado: %s", ota_cmd_result_str(r));
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_running) {
        ESP_LOGW(TAG, "cmd/ota ignorado: ya hay una OTA en curso");
    } else if (fleet) {
        uint32_t delay_s = CONFIG_INCUTWIN_OTA_FLEET_DELAY_MAX_S
                               ? esp_random() % (CONFIG_INCUTWIN_OTA_FLEET_DELAY_MAX_S + 1) : 0;
        s_pending = job;
        s_has_pending = true;
        esp_timer_stop(s_delay_timer);
        esp_timer_start_once(s_delay_timer, (uint64_t)delay_s * 1000000ULL);
        ESP_LOGI(TAG, "OTA de flota en %lu s: %s", (unsigned long)delay_s, job.url);
    } else {
        esp_timer_stop(s_delay_timer); /* un unicast sustituye al pendiente de flota */
        ESP_LOGI(TAG, "OTA unicast: %s", job.url);
        start_job_locked(&job);
    }
    xSemaphoreGive(s_mutex);
}

void ota_update_start(void)
{
    s_mutex = xSemaphoreCreateMutex();
    const esp_timer_create_args_t d = { .callback = delay_cb, .name = "ota_delay" };
    ESP_ERROR_CHECK(esp_timer_create(&d, &s_delay_timer));
    const esp_timer_create_args_t v = { .callback = validate_timeout_cb, .name = "ota_validate" };
    ESP_ERROR_CHECK(esp_timer_create(&v, &s_validate_timer));

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (running && esp_ota_get_state_partition(running, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
        s_pending_verify = true;
        esp_timer_start_once(s_validate_timer, (uint64_t)CONFIG_INCUTWIN_OTA_VALIDATE_TIMEOUT_S * 1000000ULL);
        ESP_LOGW(TAG, "firmware pendiente de verificar: %d s para conectar al broker",
                 CONFIG_INCUTWIN_OTA_VALIDATE_TIMEOUT_S);
    }
    app_events_subscribe(TWIN_EVT_MQTT, on_mqtt, NULL);
#if ALLOW_ANY_HOST
    ESP_LOGW(TAG, "BUILD DE DESARROLLO: SE ACEPTA OTA DESDE CUALQUIER HOST HTTPS");
#endif
}
