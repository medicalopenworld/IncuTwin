/* IncuTwin — punto de entrada.
 *
 * app_main solo ordena el arranque de los componentes y termina; toda la
 * logica vive en components/. Orden: NVS -> bus de eventos -> hardware ->
 * identidad y ajustes -> modelo -> sonido -> WiFi -> (onboarding | UI + demo)
 * -> tick de 1 s.
 */

#include <inttypes.h>

#include "app_events.h"
#include "board.h"
#include "bringup_screen.h"
#include "cloud_status.h"
#include "commands.h"
#include "console_cmds.h"
#include "demo_mode.h"
#include "mqtt_link.h"
#include "ota_update.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "identity.h"
#include "net_wifi.h"
#include "sdkconfig.h"
#include "settings.h"
#include "sim_server.h"
#include "sound.h"
#include "storage.h"
#include "twin_model.h"
#include "ui.h"
#include "ui_onboarding.h"
#include "usage_stats.h"

static const char *TAG = "incutwin";

static const char *ota_state_str(esp_ota_img_states_t st)
{
    switch (st) {
    case ESP_OTA_IMG_NEW:            return "new";
    case ESP_OTA_IMG_PENDING_VERIFY: return "pending-verify";
    case ESP_OTA_IMG_VALID:          return "valid";
    case ESP_OTA_IMG_INVALID:        return "invalid";
    case ESP_OTA_IMG_ABORTED:        return "aborted";
    default:                         return "undefined";
    }
}

static void log_boot_banner(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t st = ESP_OTA_IMG_UNDEFINED;
    if (running) esp_ota_get_state_partition(running, &st);
    ESP_LOGI(TAG, "IncuTwin %s SN %s", app->version, identity_sn());
    ESP_LOGI(TAG, "slot %s @0x%" PRIx32 " estado %s, idf %s", running ? running->label : "?",
             running ? running->address : 0, ota_state_str(st), app->idf_ver);
#ifdef CONFIG_INCUTWIN_OTA_ALLOW_ANY_HOST
    ESP_LOGW(TAG, "BUILD DE DESARROLLO: OTA ACEPTA CUALQUIER HOST HTTPS");
#endif
}

/* Tick de 1 s: tiempos del modelo y contadores de uso (tarea esp_timer, corto). */
static void tick_1s_cb(void *arg)
{
    (void)arg;
    twin_model_tick_1s();
    twin_snapshot_t s = twin_model_get();
    usage_tick_1s(s.link_ok && s.inc.online && !s.demo);
}

static void start_wifi_station(void)
{
    char ssid[33], pass[65];
    if (identity_wifi_creds(ssid, sizeof(ssid), pass, sizeof(pass))) {
        net_wifi_connect(ssid, pass);
    } else {
        ESP_LOGW(TAG, "sin credenciales WiFi guardadas");
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(storage_init());
    ESP_ERROR_CHECK(app_events_init());
    ESP_ERROR_CHECK(board_init());

    identity_init();
    log_boot_banner();
    settings_init();
    usage_init();

    char inc_id[TWIN_INCUBATOR_ID_LEN] = "";
    identity_incubator_id(inc_id);
    twin_model_init(CONFIG_INCUTWIN_DEFAULT_SKIN, identity_has_creds(), inc_id);
    sound_init();
    ESP_ERROR_CHECK(net_wifi_init());
#if !CONFIG_INCUTWIN_SIM
    /* el broker: arranca en cuanto haya WiFi; sin credenciales se queda en espera */
    if (mqtt_link_start() == ESP_OK) {
        cloud_status_start();
        commands_start();
    }
    ota_update_start(); /* tambien sin credenciales: arma el watchdog de rollback si procede */
#else
    sim_server_start(); /* build de simulacion: servidor web en vez de broker */
#endif

#if CONFIG_INCUTWIN_BRINGUP_SCREEN
    bringup_screen_start();
#else
    if (!identity_prov_done()) {
        ui_onboarding_start(); /* primer arranque: el asistente conecta el WiFi */
    } else {
        ui_init();
        demo_mode_start();
        start_wifi_station();
    }
#endif

    const esp_timer_create_args_t tick_args = {
        .callback = tick_1s_cb, .name = "tick_1s", .dispatch_method = ESP_TIMER_TASK,
    };
    esp_timer_handle_t tick;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick, 1000 * 1000));

    console_start();
    board_log_memory(TAG);
}
