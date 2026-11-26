#include "ota_update.h"

#include <string.h>

#include "app_events.h"
#include "esp_app_desc.h"
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
#include <stdatomic.h>
#include "ota_validate.h"
#include "psa/crypto.h" /* Mbed TLS 4: el hash va por la API PSA */
#include "sdkconfig.h"
#include "storage.h"

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
static char s_pending_version[OTA_VERSION_MAX + 1]; /* su version si viene de un objetivo */
static bool s_has_pending;
static bool s_running;
static esp_timer_handle_t s_delay_timer;
static esp_timer_handle_t s_validate_timer;
static bool s_pending_verify;
static ota_job_t s_queued;          /* listo para descargar en cuanto la UI diga (ota_update_go) */
static bool s_has_queued;
static atomic_int s_progress = OTA_PROGRESS_IDLE;

/* OTA pendiente guardada en NVS: si se corta (luz, red) se vuelve a intentar al reconectar, hasta
 * OTA_MAX_ATTEMPTS descargas. Al instalarse se marca y el siguiente arranque la borra. */
#define OTA_MAX_ATTEMPTS 3
#define KEY_JOB          "job"
typedef struct {
    ota_job_t job;
    uint8_t attempts;  /* descargas empezadas */
    uint8_t installed; /* fijada la particion: el siguiente arranque la da por hecha */
    char version[OTA_VERSION_MAX + 1]; /* la del objetivo (cmd/fw); "" si fue un cmd/ota */
} ota_saved_t;
static ota_saved_t s_saved;
static bool s_has_saved;
static bool s_retry_on_connect; /* reintentar la guardada en la proxima conexion al broker */

/* Binario que ya fallo (sin intentos, hash distinto, imagen invalida o rollback): un objetivo
 * retenido con ese sha256 no se vuelve a intentar, aunque llegue en cada conexion */
#define KEY_BAD "bad"
static uint8_t s_bad[32];
static bool s_has_bad;

/* Firmware objetivo retenido (cmd/fw): el del panel manda sobre el de la flota */
static ota_target_t s_tgt_dev, s_tgt_all;
static bool s_has_tgt_dev, s_has_tgt_all;

static void mark_bad_locked(const uint8_t sha[32], const char *why)
{
    memcpy(s_bad, sha, 32);
    s_has_bad = true;
    storage_set_blob(STORAGE_NS_OTA, KEY_BAD, s_bad, sizeof(s_bad));
    ESP_LOGW(TAG, "binario descartado para siempre: %s", why);
}

static void saved_write(void)
{
    if (!storage_set_blob(STORAGE_NS_OTA, KEY_JOB, &s_saved, sizeof(s_saved))) ESP_LOGW(TAG, "no se pudo guardar la OTA");
}

static void saved_clear(const char *why)
{
    if (s_has_saved) ESP_LOGI(TAG, "OTA guardada borrada: %s", why);
    s_has_saved = false;
    s_retry_on_connect = false;
    storage_erase_key(STORAGE_NS_OTA, KEY_JOB);
}
#define REBOOT_AFTER_MS 1500       /* «100 %» en pantalla antes de reiniciar */

/* ------------------------------------------------------------- rollback */

static void validate_timeout_cb(void *arg)
{
    (void)arg;
    if (!s_pending_verify) return;
    ESP_LOGE(TAG, "%d s sin conectar al broker: firmware marcado invalido, rollback",
             CONFIG_INCUTWIN_OTA_VALIDATE_TIMEOUT_S);
    esp_ota_mark_app_invalid_rollback_and_reboot();
}

static void queue_job_locked(const ota_job_t *job, const char *version);

static void on_mqtt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    if (!((const app_evt_mqtt_t *)data)->connected) return;
    /* OTA cortada (luz, red): se vuelve a intentar al conectar */
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_retry_on_connect && s_has_saved && !s_running && s_saved.attempts < OTA_MAX_ATTEMPTS) {
        ESP_LOGW(TAG, "reintentando la OTA cortada (%u de %d intentos hechos)", s_saved.attempts, OTA_MAX_ATTEMPTS);
        queue_job_locked(&s_saved.job, s_saved.version);
    }
    xSemaphoreGive(s_mutex);
    if (!s_pending_verify) return;
    s_pending_verify = false;
    esp_timer_stop(s_validate_timer);
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGI(TAG, "app marked valid (%s)", esp_err_to_name(err));
}

/* ------------------------------------------------------------- download */

static void ota_fail(esp_ota_handle_t h, esp_http_client_handle_t http, const char *why)
{
    ESP_LOGE(TAG, "OTA abortada: %s", why);
    atomic_store(&s_progress, OTA_PROGRESS_FAILED);
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
#if ALLOW_ANY_HOST && CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY
        .crt_bundle_attach = NULL, /* SOLO build de prueba de OTA: servidor local sin certificado publico */
#else
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif
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
    atomic_store(&s_progress, 0);
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
        if (total > 0) atomic_store(&s_progress, (int)(received * 99 / total)); /* 100 = instalada */
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
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        mark_bad_locked(job.sha256, "sha256 distinto");
        saved_clear("sha256 distinto (no se reintenta)");
        xSemaphoreGive(s_mutex);
        goto done;
    }
    ESP_LOGI(TAG, "sha256 correcto (%lld B)", (long long)received);
    esp_err_t err = esp_ota_end(handle);
    handle = 0;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end: %s (imagen invalida)", esp_err_to_name(err));
        atomic_store(&s_progress, OTA_PROGRESS_FAILED);
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        mark_bad_locked(job.sha256, "imagen invalida");
        saved_clear("imagen invalida (no se reintenta)");
        xSemaphoreGive(s_mutex);
        goto done;
    }
    if (esp_ota_set_boot_partition(target) != ESP_OK) {
        ESP_LOGE(TAG, "no se pudo fijar la particion de arranque");
        atomic_store(&s_progress, OTA_PROGRESS_FAILED);
        goto done;
    }
    /* la ventana ya esta en pantalla: «100 %» un momento y reinicio */
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_has_saved) {
        s_saved.installed = 1; /* el siguiente arranque la da por hecha */
        saved_write();
    }
    xSemaphoreGive(s_mutex);
    atomic_store(&s_progress, 100);
    ESP_LOGW(TAG, "firmware instalado en %s: reiniciando", target->label);
    vTaskDelay(pdMS_TO_TICKS(REBOOT_AFTER_MS));
    esp_restart();

done:
    if (sha_active) psa_hash_abort(&sha);
    free(buf);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_running = false;
    if (s_has_saved) { /* fallo de red o similar: otra vez en la proxima conexion, si quedan intentos */
        if (s_saved.attempts >= OTA_MAX_ATTEMPTS) {
            mark_bad_locked(s_saved.job.sha256, "sin intentos");
            saved_clear("sin intentos");
        } else {
            s_retry_on_connect = true;
        }
    }
    xSemaphoreGive(s_mutex);
    vTaskDelete(NULL);
}

/* Listo para descargar: espera a que la UI abra su ventana (TWIN_EVT_OTA_PENDING -> ota_update_go) */
static void queue_job_locked(const ota_job_t *job, const char *version)
{
    /* una OTA nueva (otro binario) empieza su cuenta de intentos */
    if (!s_has_saved || memcmp(s_saved.job.sha256, job->sha256, 32) != 0 || strcmp(s_saved.job.url, job->url) != 0) {
        memset(&s_saved, 0, sizeof(s_saved));
        s_saved.job = *job;
        if (version) strncpy(s_saved.version, version, OTA_VERSION_MAX);
        s_has_saved = true;
        saved_write();
    }
    s_retry_on_connect = false;
    s_queued = *job;
    s_has_queued = true;
    s_has_pending = false;
    s_running = true; /* desde aqui no se acepta otra OTA */
    ESP_LOGI(TAG, "OTA lista: esperando a la pantalla");
    app_events_post(TWIN_EVT_OTA_PENDING, NULL, 0);
}

static void delay_cb(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_has_pending && !s_running) {
        ESP_LOGI(TAG, "retardo de flota vencido");
        queue_job_locked(&s_pending, s_pending_version);
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
        s_pending_version[0] = '\0';
        s_has_pending = true;
        esp_timer_stop(s_delay_timer);
        esp_timer_start_once(s_delay_timer, (uint64_t)delay_s * 1000000ULL);
        ESP_LOGI(TAG, "OTA de flota en %lu s: %s", (unsigned long)delay_s, job.url);
    } else {
        esp_timer_stop(s_delay_timer); /* un unicast sustituye al pendiente de flota */
        ESP_LOGI(TAG, "OTA unicast: %s", job.url);
        queue_job_locked(&job, NULL);
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

    s_has_bad = storage_get_blob(STORAGE_NS_OTA, KEY_BAD, s_bad, sizeof(s_bad));
    /* OTA guardada de antes del reinicio: instalada = hecha; si no, se reintenta al conectar */
    if (storage_get_blob(STORAGE_NS_OTA, KEY_JOB, &s_saved, sizeof(s_saved))) {
        s_has_saved = true;
        if (s_saved.installed) {
            /* vino de un objetivo y no corre esa version: el firmware nuevo no arranco (rollback) */
            const char *running = esp_app_get_description()->version;
            if (s_saved.version[0] && ota_version_cmp(running, s_saved.version) != 0) {
                mark_bad_locked(s_saved.job.sha256, "rollback: el firmware nuevo no llego a conectar");
            }
            saved_clear("instalada antes de este arranque");
        } else if (s_saved.attempts >= OTA_MAX_ATTEMPTS) {
            saved_clear("sin intentos");
        } else {
            s_retry_on_connect = true;
            ESP_LOGW(TAG, "OTA cortada antes de este arranque (%u intentos): se reintenta al conectar",
                     s_saved.attempts);
        }
    }

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

bool ota_update_pending(void)
{
    if (!s_mutex) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool p = s_has_queued;
    xSemaphoreGive(s_mutex);
    return p;
}

void ota_update_go(void)
{
    static ota_job_t job_copy; /* la tarea la copia al arrancar */
    if (!s_mutex) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_has_queued) {
        s_has_queued = false;
        job_copy = s_queued;
        if (s_has_saved) {
            s_saved.attempts++;
            saved_write();
            ESP_LOGI(TAG, "intento %u de %d", s_saved.attempts, OTA_MAX_ATTEMPTS);
        }
        atomic_store(&s_progress, 0);
        if (xTaskCreatePinnedToCore(ota_task, "ota_task", 8192, &job_copy, 3, NULL, 0) != pdPASS) {
            s_running = false;
            atomic_store(&s_progress, OTA_PROGRESS_FAILED);
            ESP_LOGE(TAG, "no se pudo crear la tarea OTA");
        } else {
            ESP_LOGI(TAG, "descarga lanzada desde la pantalla");
        }
    }
    xSemaphoreGive(s_mutex);
}

int ota_update_progress(void) { return atomic_load(&s_progress); }

/* --------------------------------------------------------- firmware objetivo */

/* El objetivo que manda (el del panel sobre el de la flota) y, si el panel va por detras, la OTA:
 * la del panel en seguida; la de la flota con el retardo aleatorio de siempre. */
static void target_eval_locked(void)
{
    const ota_target_t *t = s_has_tgt_dev ? &s_tgt_dev : s_has_tgt_all ? &s_tgt_all : NULL;
    bool fleet = !s_has_tgt_dev;
    if (!t) return;
    const char *running = esp_app_get_description()->version;
    if (!ota_target_wanted(running, t, s_has_bad ? s_bad : NULL)) {
        ESP_LOGI(TAG, "objetivo %s (%s): nada que hacer con %s%s", t->version, fleet ? "flota" : "panel", running,
                 s_has_bad && !memcmp(s_bad, t->job.sha256, 32) ? " (ese binario ya fallo)" : "");
        if (s_has_pending && !s_running) { /* un objetivo de flota pendiente que el del panel anula */
            esp_timer_stop(s_delay_timer);
            s_has_pending = false;
        }
        return;
    }
    if (s_running) {
        ESP_LOGW(TAG, "objetivo %s: ya hay una OTA en curso", t->version);
        return;
    }
    if (fleet) {
        uint32_t delay_s = CONFIG_INCUTWIN_OTA_FLEET_DELAY_MAX_S
                               ? esp_random() % (CONFIG_INCUTWIN_OTA_FLEET_DELAY_MAX_S + 1) : 0;
        s_pending = t->job;
        strncpy(s_pending_version, t->version, OTA_VERSION_MAX);
        s_has_pending = true;
        esp_timer_stop(s_delay_timer);
        esp_timer_start_once(s_delay_timer, (uint64_t)delay_s * 1000000ULL);
        ESP_LOGI(TAG, "objetivo de flota %s (llevo %s): OTA en %lu s", t->version, running, (unsigned long)delay_s);
    } else {
        esp_timer_stop(s_delay_timer);
        s_has_pending = false;
        ESP_LOGI(TAG, "objetivo del panel %s (llevo %s): OTA ya", t->version, running);
        queue_job_locked(&t->job, t->version);
    }
}

void ota_update_target(const char *payload, size_t len, bool fleet)
{
    if (!s_mutex) return;
    ota_target_t t;
    bool clear = len == 0;
    if (!clear) {
        ota_cmd_result_t r = ota_validate_target(payload, len, CONFIG_INCUTWIN_OTA_HOST_SUFFIX, ALLOW_ANY_HOST, &t);
        if (r != OTA_CMD_OK) {
            ESP_LOGW(TAG, "cmd/fw rechazado: %s", ota_cmd_result_str(r));
            return;
        }
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (fleet) {
        s_has_tgt_all = !clear;
        if (!clear) s_tgt_all = t;
    } else {
        s_has_tgt_dev = !clear;
        if (!clear) s_tgt_dev = t;
    }
    if (clear) ESP_LOGI(TAG, "objetivo %s borrado", fleet ? "de la flota" : "del panel");
    target_eval_locked();
    xSemaphoreGive(s_mutex);
}
