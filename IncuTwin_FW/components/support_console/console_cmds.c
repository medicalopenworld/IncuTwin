#include "console_cmds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app_events.h"
#include "board.h"
#include "demo_mode.h"
#include "esp_app_desc.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "identity.h"
#include "incupray.h"
#include "rewards.h"
#include "ui_debug.h"
#include "sdkconfig.h"
#include "telemetry.h"
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
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    if (tm.tm_year + 1900 >= 2025) {
        printf("hora       %04d-%02d-%02d %02d:%02d:%02d (local)\n", tm.tm_year + 1900, tm.tm_mon + 1,
               tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    } else {
        printf("hora       sin SNTP\n");
    }
    board_log_memory(TAG);
    return 0;
}

/* Telemetria de uso: identidades, cola y lote en vuelo (soporte y verificacion) */
static int cmd_tel(int argc, char **argv)
{
    (void)argc; (void)argv;
    telemetry_print_status();
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

/* Solo lectura: lo que lleva ganado este panel (soporte y verificacion) */
/* Captura de la pantalla para tools/screenshot.py */
static int cmd_screen(int argc, char **argv)
{
    (void)argc; (void)argv;
    return ui_debug_screenshot() == 0 ? 0 : 1;
}

/* tienda <gorros|mantas|fondos|efectos|corazones|cerrar> [nina] [probar <id>] [comprar] */
static int cmd_shop(int argc, char **argv)
{
    if (argc < 2) {
        printf("tienda <gorros|mantas|fondos|efectos|corazones|cerrar> [nina] [probar <id>] [comprar]\n");
        return 1;
    }
    bool girl = false, buy = false;
    int item = -1;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "nina") == 0) girl = true;
        else if (strcmp(argv[i], "comprar") == 0) buy = true;
        else if (strcmp(argv[i], "probar") == 0 && i + 1 < argc) item = atoi(argv[++i]);
    }
    if (!ui_debug_shop(argv[1], girl, item, buy)) {
        printf("pestaña desconocida: %s\n", argv[1]);
        return 1;
    }
    return 0;
}

/* ir <pantalla>: revisar el estilo de cada pantalla con tools/screenshot.py */
static int cmd_goto(int argc, char **argv)
{
    if (argc < 2 || !ui_debug_goto(argv[1])) {
        printf("ir inicio|avisos|corazones|historial|ajustes|bebe|mapa|racha|listo|dedo|soltar|fin\n");
        return 1;
    }
    return 0;
}

static int cmd_nowifi(int argc, char **argv)
{
    if (argc < 2 || !ui_debug_nowifi(argv[1])) {
        printf("sinwifi on|nina|off|asistente\n");
        return 1;
    }
    return 0;
}

static int cmd_demo(int argc, char **argv)
{
    if (argc < 2 || (strcmp(argv[1], "on") && strcmp(argv[1], "off"))) {
        printf("demo on|off\n");
        return 1;
    }
    demo_mode_button(strcmp(argv[1], "on") == 0); /* como el boton: primer escenario y pitido */
    return 0;
}

static int cmd_rewards(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "comprar") == 0) { /* solo en la demo: no gasta corazones reales */
        bool demo = false;
        rw_state_t tmp;
        rewards_copy(&tmp, &demo);
        if (!demo) {
            printf("solo en la demo (demo on)\n");
            return 1;
        }
        uint16_t missing = 0;
        rw_unlock_t res = rewards_unlock((rw_item_t)atoi(argv[2]), &missing);
        printf("comprar %s: %d (faltan %u)\n", argv[2], (int)res, missing);
        return res == RW_UNLOCK_OK ? 0 : 1;
    }
    if (argc >= 3 && strcmp(argv[1], "fecha") == 0) { /* pruebas de Navidad: no se guarda */
        int m = atoi(argv[2]), d = argc >= 4 ? atoi(argv[3]) : 1;
        rewards_force_date(m, d);
        if (m) printf("fecha forzada: %d/%d\n", d, m);
        else printf("fecha de verdad\n");
        return 0;
    }
    rw_state_t r;
    bool demo = false;
    rewards_copy(&r, &demo);
    printf("%ssaldo=%u ganados=%lu nivel=%d racha=%u abrazos=%u dias=%u bebes=%u sellos=0x%02x\n",
           demo ? "(demo) " : "", r.balance, (unsigned long)r.earned, (int)rw_level(r.earned), r.streak, r.saved,
           r.total_days, r.babies, r.seals);
    printf("ultimo_dia=%ld super_dia=%ld desbloqueados=0x%llx halo=%u fondo=%u guirnalda=%u\n",
           (long)r.last_day, (long)r.super_day, (unsigned long long)r.unlocked, r.halo, r.bg, r.garland_on);
    printf("gorro_nino=%u gorro_nina=%u manta=%u papa_noel=%u copos=%u pasaporte=", r.hat_boy, r.hat_girl,
           r.blanket, r.santa_on, r.snow_on);
    for (int i = 0; i < r.n_countries; i++) printf("%c%c%s ", r.passport[i].iso[0], r.passport[i].iso[1],
                                                   r.passport[i].star ? "*" : "");
    printf("\n");
    return 0;
}

/* IncuPray (spec incupray): estado; "demo" la simula 3 min dentro de la demo (corazones de mentira),
 * "stop" la corta; "hold <s>" suma segundos de dedo (solo dentro y con la ventana abierta) */
static int cmd_incupray(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "demo")) {
        demo_mode_set(true);
        incupray_demo_start();
        printf("IncuPray de la demo: 3 min\n");
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "stop")) {
        incupray_demo_stop();
        demo_mode_set(false);
        printf("demo cortada\n");
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "hold")) {
        incupray_hold((uint32_t)atoi(argv[2]) * 1000u);
    }
    if (argc >= 2 && !strcmp(argv[1], "join")) { /* como tocar «IncuPray» (cobra la apuesta la 1.a vez) */
        uint16_t paid = 0;
        printf("dentro%s (apuesta %u)\n", incupray_join(&paid) ? " por primera vez" : "", paid);
    }
    if (argc >= 2 && !strcmp(argv[1], "leave")) incupray_leave();
    ip_window_t w;
    uint32_t now = incupray_now();
    if (!incupray_window(&w)) {
        printf("sin horario (IncuPray apagada) now=%lu\n", (unsigned long)now);
        return 0;
    }
    static const char *const PH[] = { "antes", "abierta", "cerrada" };
    printf("%sid=%s inicio=%lu dura=%lus now=%lu fase=%s quedan=%lus dentro=%d dedo=%lus corazones=%u\n",
           incupray_demo_active() ? "(demo) " : "", w.id, (unsigned long)w.start, (unsigned long)w.duration_s,
           (unsigned long)now, PH[ip_phase(&w, now)], (unsigned long)ip_remaining_s(&w, now), incupray_joined(),
           (unsigned long)incupray_held_s(), incupray_hearts());
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
    demo_mode_factory_reset();
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

#if CONFIG_INCUTWIN_SIM
/* Solo build sim: mete un JSON de estado por el mismo camino que POST /incubator del servidor
 * sim, para probar avisos y sonido por serie cuando la placa no es alcanzable por HTTP. */
static int cmd_sim_state(int argc, char **argv)
{
    if (argc != 2) {
        printf("uso: sim_state \"<json de estado con incubator_id SIM>\"\n");
        return 1;
    }
    if (!strcmp(argv[1], "demo-on")) { /* la demo de verdad se activa con pulsacion larga */
        demo_mode_set(true);
        return 0;
    }
    if (!strcmp(argv[1], "demo-off")) {
        demo_mode_set(false);
        return 0;
    }
    twin_model_ingest(argv[1], strlen(argv[1]));
    return 0;
}
#endif

void console_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "incutwin>";
#if CONFIG_INCUTWIN_SIM
    repl_cfg.max_cmdline_length = 256; /* cabe un JSON de sim_state */
#else
    repl_cfg.max_cmdline_length = 64;
#endif
    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    if (esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl) != ESP_OK) {
        ESP_LOGW(TAG, "sin consola");
        return;
    }
    esp_console_register_help_command();
    const esp_console_cmd_t cmds[] = {
        { .command = "info", .help = "identidad, enlace y estado", .func = cmd_info },
        { .command = "usage", .help = "contadores de uso", .func = cmd_usage },
        { .command = "tel", .help = "telemetria: gen, boot_n, next_seq, pending, lost, lote en vuelo y pila", .func = cmd_tel },
        { .command = "rewards", .help = "corazones, racha, sellos, pasaporte y tienda; `rewards fecha <mes> <dia>` fuerza la fecha de Navidad (0 = la de verdad); `rewards comprar <id>` solo en la demo", .func = cmd_rewards },
        { .command = "demo", .help = "demo on|off: como 2 s del boton BOOT", .func = cmd_demo },
        { .command = "pantalla", .help = "captura de la pantalla (para tools/screenshot.py)", .func = cmd_screen },
        { .command = "tienda", .help = "tienda <gorros|mantas|fondos|efectos|corazones|cerrar> [nina] [probar <id>] [comprar]",
          .func = cmd_shop },
        { .command = "incupray", .help = "incupray [demo|stop|join|leave|hold <s>]: estado de la IncuPray", .func = cmd_incupray },
        { .command = "sinwifi", .help = "sinwifi on|nina|off|asistente: bebe sin WiFi en Inicio como si no hubiera WiFi; off vuelve a lo de verdad (con WiFi, celebra); asistente simula el asistente probando la WiFi",
          .func = cmd_nowifi },
        { .command = "ir", .help = "ir <inicio|avisos|corazones|historial|ajustes|bebe|mapa|racha|listo|dedo|soltar|fin>: abre una pantalla o un paso del gesto (capturas)",
          .func = cmd_goto },
        { .command = "unpair", .help = "borra el incubator_id local", .func = cmd_unpair },
        { .command = "reboot", .help = "reinicia el panel", .func = cmd_reboot },
        { .command = "factory-reset", .help = "borra WiFi y ajustes; vuelve al onboarding",
          .func = cmd_factory_reset },
        { .command = "wifi", .help = "wifi <ssid> [pass]: guarda la WiFi y salta el onboarding",
          .func = cmd_wifi },
#if CONFIG_INCUTWIN_SIM
        { .command = "sim_state", .help = "SOLO EXISTE EN EL BUILD SIM: sim_state \"<json>\" ingesta un estado; demo-on/demo-off",
          .func = cmd_sim_state },
#endif
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        esp_console_cmd_register(&cmds[i]);
    }
    esp_console_start_repl(repl);
}
