#include "console_cmds.h"

#include <stdio.h>
#include <string.h>

#include "app_events.h"
#include "board.h"
#include "esp_app_desc.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "identity.h"
#include "storage.h"
#include "twin_model.h"
#include "usage_stats.h"

static const char *TAG = "console";

static int cmd_info(int argc, char **argv)
{
    (void)argc; (void)argv;
    twin_snapshot_t s = twin_model_get();
    const esp_partition_t *run = esp_ota_get_running_partition();
    printf("sn         %s (hwrev '%s', nvs fabrica %s)\n", identity_sn(), identity_hwrev(),
           identity_has_factory_nvs() ? "si" : "no");
    printf("mqtt user  %s\n", identity_has_creds() ? identity_mqtt_user() : "(sin credenciales)");
    printf("incubator  %s\n", s.link.paired ? s.link.incubator_id : "-");
    printf("fw         %s  slot %s\n", esp_app_get_description()->version, run ? run->label : "?");
    printf("wifi       %s rssi %d ip %s\n", s.link.wifi ? "si" : "no", s.link.rssi, s.link.ip);
    printf("broker     %s%s%s\n", s.link.broker_connected ? "conectado" : "desconectado",
           s.link.broker_once ? "" : " (nunca)", s.link.broker_lost ? " PERDIDO" : "");
    printf("estado     online=%d baby=%d thermo=%d photo=%d bpm=%u state_rx=%d demo=%d\n",
           s.inc.online, s.inc.baby, s.inc.thermo, s.inc.photo, s.inc.bpm, s.link.state_rx, s.demo);
    printf("tactil     errores i2c %lu\n", (unsigned long)board_touch_error_count());
    board_log_memory(TAG);
    return 0;
}

static int cmd_usage(int argc, char **argv)
{
    (void)argc; (void)argv;
    usage_stats_t u = usage_get();
    printf("on_s=%lu conn_s=%lu hand_s=%lu hand_n=%lu\n", (unsigned long)u.on_s,
           (unsigned long)u.conn_s, (unsigned long)u.hand_s, (unsigned long)u.hand_n);
    return 0;
}

static int cmd_unpair(int argc, char **argv)
{
    (void)argc; (void)argv;
    identity_set_incubator_id(NULL);
    app_evt_pairing_t ev = { .incubator_id = "" };
    app_events_post(TWIN_EVT_PAIRING_CHANGED, &ev, sizeof(ev));
    printf("desemparejado localmente (el cmd/pair retenido volvera a emparejar)\n");
    return 0;
}

static int cmd_reboot(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("reiniciando...\n");
    fflush(stdout);
    esp_restart();
    return 0;
}

static int cmd_factory_reset(int argc, char **argv)
{
    (void)argc; (void)argv;
    storage_factory_reset();
    return 0;
}

/* Banco de pruebas: configura la WiFi por serie y da el onboarding por hecho, como
 * si la familia hubiera pasado por el portal. Quien tiene el USB puede flashear
 * lo que quiera, asi que no abre nada que no estuviera ya abierto. */
static int cmd_wifi(int argc, char **argv)
{
    if (argc < 2 || argc > 3) {
        printf("uso: wifi <ssid> [contrasena]\n");
        return 1;
    }
    identity_set_wifi_creds(argv[1], argc == 3 ? argv[2] : "");
    identity_prov_mark_done();
    printf("wifi '%s' guardada y onboarding marcado como hecho; reinicia para aplicar\n", argv[1]);
    return 0;
}

void console_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "incutwin>";
    repl_cfg.max_cmdline_length = 64;
    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    if (esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl) != ESP_OK) {
        ESP_LOGW(TAG, "sin consola");
        return;
    }
    esp_console_register_help_command();
    const esp_console_cmd_t cmds[] = {
        { .command = "info", .help = "identidad, enlace y estado", .func = cmd_info },
        { .command = "usage", .help = "contadores de uso", .func = cmd_usage },
        { .command = "unpair", .help = "borra el incubator_id local", .func = cmd_unpair },
        { .command = "reboot", .help = "reinicia el panel", .func = cmd_reboot },
        { .command = "factory-reset", .help = "borra WiFi y ajustes; vuelve al onboarding",
          .func = cmd_factory_reset },
        { .command = "wifi", .help = "wifi <ssid> [pass]: guarda la WiFi y salta el onboarding",
          .func = cmd_wifi },
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        esp_console_cmd_register(&cmds[i]);
    }
    esp_console_start_repl(repl);
}
