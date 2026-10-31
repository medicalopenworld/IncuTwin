# idf-port — Diseño

## Context

Firmware actual: Arduino 2.0.x sobre PlatformIO (IDF 4.4), ~6.300 líneas, un `g_state` global
con mutex y flag `dirty`, lógica de negocio dentro de `ui_main.cpp`, cliente ThingsBoard sin TLS.
Decisiones de producto ya cerradas: solo broker Mosquitto (`docs/BROKER-MQTT-CONTEXT.md`), OTA
con rollback, Secure Boot v2 antes de fabricar. Hardware fijo: CrowPanel Advance 2.8"
(ESP32-S3-WROOM-1-N16R8, flash 16 MB QIO, PSRAM 8 MB octal, ST7789 SPI, FT5x06 I2C).
Herramientas disponibles en la máquina de Pablo: ESP-IDF 6.0.1 nativo (`C:\esp\v6.0.1`), gcc
15.2, CMake 4.0, Ninja 1.12, Python 3.12, una IncuTwin en COM12, WiFi de laboratorio `in3wifi`.

Restricción de método: el código Arduino es referencia de comportamiento (ya volcado a las
specs), no se porta. Partes neutrales que se conservan: `Images/`, `docs/`, y los scripts de
`tools/` (adaptados).

## Goals / Non-Goals

**Goals:**

- Un proyecto ESP-IDF idiomático: componentes con header público, Kconfig para opciones de
  producto, `esp_event` para desacoplar, FreeRTOS explícito, logs `ESP_LOGx`.
- Cumplir las 18 specs del change con verificación en el panel real en cada corte vertical.
- Dejar el proyecto listo para Secure Boot v2 y cifrado de flash (bootloader propio, Kconfig),
  sin quemar eFuses todavía.
- Lógica de negocio (modelo del gemelo, parser, dedup, validación de comandos, backoff) en C
  puro sin dependencias de IDF, probada con Unity on-target.

**Non-Goals:**

- Publicar "Coge mi mano" por MQTT (sin topic en el contrato).
- Secure Boot / flash encryption activados (decisión de fabricación).
- Tests host (Linux target): en Windows requiere WSL; se sustituyen por `test_apps` on-target.
- Soporte de GT911 (algunos lotes de Elecrow): la unidad disponible lleva FT5x06; se deja el
  táctil detrás de `board` para añadirlo después.
- Compatibilidad con la NVS `prov` antigua (token TB, nombre, email): se borra con el
  `erase_flash` obligatorio por el cambio de particiones.

## Decisions

### D1. ESP-IDF 6.0.1 con `idf.py` nativo (no PlatformIO `espidf`)
PlatformIO ofrece IDF 5.1.2 y su integración va por detrás (sdkconfig, componentes gestionados,
rutas largas en Windows). Con `idf.py` se usa la versión instalada, el gestor de componentes y
`idf.py menuconfig/size/monitor` tal cual. Riesgo asumido: 6.0 es reciente; si un componente del
registro no compila, el plan B es pinear IDF 5.5 LTS sin cambiar código de aplicación (las APIs
usadas —esp_lcd, esp-mqtt, esp_http_client, esp_ota_ops, nvs, esp_event— son estables entre 5.x
y 6.0).

### D2. `esp_lcd` + `esp_lcd_touch_ft5x06` + `esp_lvgl_port` (no LovyanGFX)
Componentes mantenidos por Espressif, con soporte garantizado en IDF 6.0 y LVGL 9. LovyanGFX es
C++ con su propia abstracción de bus, sin garantía en 6.0 y solapa con esp_lcd. Coste: hay que
re-verificar orientación y mapeo táctil en hardware (tarea 1). Configuración de partida, derivada
de la actual (`offset_rotation = 2` en LovyanGFX = 180°): `esp_lcd_panel_mirror(panel, true,
true)`, `swap_xy = false`, `invert_color = true`, `rgb_order = RGB`; táctil: `mirror_x = true`
(equivalente al `240 - x` actual), `mirror_y = false`, `swap_xy = false`. Se ajusta empíricamente
con la pantalla de dianas del bring-up.

### D3. LVGL 9.6 vía esp_lvgl_port, render parcial con doble buffer en RAM interna
`esp_lvgl_port` crea la tarea de LVGL, el tick (esp_timer) y el `lvgl_port_lock()`. Buffers:
2 × (240 × 64 px × 2 B) = 60 KB en RAM interna DMA (`LV_DISPLAY_RENDER_MODE_PARTIAL`); la
alternativa de framebuffer completo en PSRAM (150 KB × 2) tiene DMA más lento desde PSRAM y no
hace falta para 240×320. LVGL 9 cambia el formato de imagen: `gen_assets.py` genera
`lv_image_dsc_t` con `LV_COLOR_FORMAT_I8` (bebé, paleta intercambiable en PSRAM, igual que hoy),
`ARGB8888` para iconos con alfa y `RGB565` para las dos imágenes a pantalla completa. Fuentes:
se regeneran con `lv_font_conv` para v9 con la misma línea de comandos que consta en la cabecera
de las fuentes actuales (Montserrat Medium 12/14/16/20 + glifos ES + símbolos FontAwesome).

### D4. Un bus de eventos propio sobre `esp_event` (no `g_state` global)
Loop dedicado `incutwin_evt` con base `TWIN_EVENTS`. Productores: `net_wifi`, `mqtt_link`,
`twin_model`, `pairing`, `demo_mode`, `sim_server`. Consumidores: `ui`, `sound`, `usage_stats`,
`cloud_status`, `ota_update`. El modelo del gemelo (`twin_model`) es el único dueño del estado:
recibe mensajes crudos, los valida, calcula transiciones y publica `TWIN_STATE_CHANGED` con una
copia inmutable del estado (`twin_snapshot_t`, ~96 B por valor) y `TWIN_TRANSITION` con el tipo
(`BABY_IN`, `BABY_PARENTS`). La UI copia el snapshot bajo `lvgl_port_lock()`. Alternativa
descartada: mutex + flag `dirty` + polling (lo actual): acopla la UI a todos los productores y
hace imposible testear el modelo sin LVGL.

Flujo de eventos:

```
esp_wifi/esp_netif ─► net_wifi ──WIFI_UP/DOWN(rssi, ip)──────────────┐
esp-mqtt task ─────► mqtt_link ──MQTT_CONNECTED/DISCONNECTED─────────┤
                     mqtt_link ──MQTT_MSG(topic, payload) ──► commands ─► pairing / ota_update / settings / sound
                                                            └► twin_model ──TWIN_STATE_CHANGED(snapshot)──► ui, sound, usage_stats
                                                                          ──TWIN_TRANSITION(kind)────────► sound
demo_mode ─────────► twin_model (overlay de presentación: DEMO_ENTER/EXIT/SCENARIO)
net_wifi, mqtt_link, pairing ──LINK_CHANGED──► twin_model (recalcula link_ok) ─► ui
mqtt_link ──MQTT_CONNECTED──► cloud_status (publica), ota_update (mark valid)
```

### D5. Modelo de tareas FreeRTOS

| Tarea | Origen | Core | Prio | Stack | Función |
|---|---|---|---|---|---|
| `main` | IDF | 0 | 1 | 4 KB | init ordenado y termina (`app_main` retorna) |
| `lvgl` | esp_lvgl_port | 1 | 4 | 8 KB | render, input, timers de UI, animaciones; `sound` corre aquí vía `lv_timer` 15 ms |
| `incutwin_evt` | esp_event loop propio | 0 | 5 | 6 KB | despacho de eventos de la app (handlers cortos, < 5 ms) |
| `mqtt_task` | esp-mqtt | 0 | 5 | 8 KB | TLS + MQTT; el handler solo encola eventos |
| `ota_task` | ota_update (creada bajo demanda) | 0 | 3 | 8 KB | descarga + sha256; se destruye al acabar |
| `portal_http` | esp_http_server | 0 | 5 | 6 KB | solo durante el onboarding |
| `dns_task` | captive_portal | 0 | 4 | 3 KB | respondedor DNS UDP, solo durante el onboarding |
| `sim_http` | esp_http_server | 0 | 5 | 6 KB | solo `CONFIG_INCUTWIN_SIM` |
| `tick_1s` | esp_timer (callback) | — | — | — | RSSI, `broker_lost`, usage tick, watchdog de validación OTA |
| `button` | esp_timer 10 ms (callback) | — | — | — | muestreo BOOT con antirrebote |

Núcleo 1 queda para LVGL (la carga gráfica); la pila de red (lwIP, WiFi, mbedTLS) y la app en el
0. `CONFIG_FREERTOS_HZ=1000`. Ningún handler de `esp_event`, callback de esp-mqtt ni ISR hace
trabajo largo: encolan y salen.

### D6. esp-mqtt con backoff propio
`esp_mqtt_client_config_t`: `broker.address.uri` = Kconfig, `broker.verification.crt_bundle_attach
= esp_crt_bundle_attach`, `credentials.client_id/username/authentication.password` desde NVS,
`session.keepalive = 60`, `session.disable_clean_session = false`, `session.last_will` con topic,
`{"online":false}`, QoS 1, retain, `network.disable_auto_reconnect = true`. La reconexión la
dirige `mqtt_link` con un `esp_timer` one-shot y backoff 1→60 s ±20 % (lógica en C puro,
testeable). Alternativa descartada: `reconnect_timeout_ms` fijo de esp-mqtt (no es exponencial,
que es lo que pide el contrato para 50.000 unidades).

### D7. OTA con `esp_http_client` + `esp_ota_ops` + `mbedtls_sha256`
`esp_https_ota` no expone el flujo de bytes para hashearlo antes de activar la partición. Con
`esp_http_client` en modo streaming se escribe con `esp_ota_write` y se acumula el sha256; al
final se compara y solo entonces `esp_ota_end` + `esp_ota_set_boot_partition`. Mismo rollback del
bootloader. El watchdog de validación (900 s sin CONNACK → `esp_ota_mark_app_invalid_rollback_and_reboot`)
vive en `ota_update` y escucha `MQTT_CONNECTED`.

### D8. cJSON
Nativo en IDF, suficiente para payloads ≤ 1 KB; evita C++ en la capa de red y deja el parser del
modelo en C puro. ArduinoJson funciona en IDF pero es C++ header-only y rompe la regla "nada de
Arduino".

### D9. Presentación de la demo como overlay, no como escritura en el modelo
`demo_mode` pide a `twin_model` entrar en modo overlay: el modelo sigue aplicando mensajes reales
a su estado interno, pero el snapshot que publica es el del escenario (con `link_ok` forzado y la
bandera `demo`). Al salir publica el snapshot real. Así "al salir se ve el estado actual" sale
gratis y la red nunca se entera de la demo. Las transiciones sonoras de la demo las genera el
modelo igual que con mensajes reales.

### D10. Kconfig para producto, `sdkconfig.defaults` para plataforma
`Kconfig.projbuild` (menú "IncuTwin"): `INCUTWIN_MQTT_URI`, `INCUTWIN_MQTT_KEEPALIVE_S`,
`INCUTWIN_BACKOFF_MAX_S`, `INCUTWIN_STATUS_PERIOD_S`, `INCUTWIN_BROKER_LOST_S`,
`INCUTWIN_OTA_HOST_SUFFIX`, `INCUTWIN_OTA_ALLOW_ANY_HOST` (dev), `INCUTWIN_OTA_FLEET_DELAY_MAX_S`,
`INCUTWIN_OTA_VALIDATE_TIMEOUT_S`, `INCUTWIN_DEFAULT_SKIN`, `INCUTWIN_SNTP_SERVER`,
`INCUTWIN_SIM` (build de simulación; `sdkconfig.defaults.sim` la activa).
`sdkconfig.defaults`: target esp32s3, flash 16 MB QIO 80 MHz, `SPIRAM_MODE_OCT` + `SPIRAM_SPEED_80M`,
`PARTITION_TABLE_CUSTOM` (`partitions.csv`), `BOOTLOADER_APP_ROLLBACK_ENABLE=y`,
`MBEDTLS_CERTIFICATE_BUNDLE=y` (completo), `ESP_TLS_SERVER_NAME_INDICATION` implícito,
`LWIP_SNTP`, `FREERTOS_HZ=1000`, `ESP_CONSOLE_UART_DEFAULT` 115200, `LOG_DEFAULT_LEVEL_INFO`,
`ESP_COREDUMP_ENABLE_TO_FLASH`, `COMPILER_OPTIMIZATION_SIZE` en release. Secure Boot y flash
encryption documentados en `docs/SECURE_BOOT.md` como procedimiento de fabricación, desactivados.

### D11. Estructura de componentes

```
CMakeLists.txt  sdkconfig.defaults  sdkconfig.defaults.sim  partitions.csv  Kconfig.projbuild
version.txt (PROJECT_VER)  idf_component.yml (lvgl/lvgl ^9.6, espressif/esp_lvgl_port ^2.9,
                                              espressif/esp_lcd_touch_ft5x06 ^1.1)
main/                 app_main.c: nvs → events → board → settings → ui → (onboarding | wifi+mqtt+cmds+ota+demo | sim)
components/
  board/              pines (único sitio), esp_lcd ST7789, touch, backlight, buzzer, button, psram report
  app_events/         base TWIN_EVENTS, ids, structs de payload, loop propio
  twin_model/         twin_snapshot_t, parser cJSON del payload, dedup, transiciones, overlay demo   [C puro + cJSON]
  storage/            nvs wrapper por namespace, factory reset selectivo
  identity/           sn, hwrev, credenciales mqtt
  settings/           lang, vol, bright (+ aplica brillo en board)
  usage_stats/
  net_wifi/           STA, reconexión con backoff, RSSI 1 s, SNTP
  captive_portal/     SoftAP, DNS, esp_http_server, páginas i18n
  mqtt_link/          esp-mqtt, LWT, suscripciones, backoff, eventos                                    [backoff en C puro]
  cloud_status/       payload status, cadencia
  pairing/            cmd/pair → nvs + (un)subscribe
  commands/           despacho cmd/#, validación, reboot/test_melody/brightness, origen unicast/all     [validación en C puro]
  ota_update/         validación URL/sha, cola de un job, retardo flota, descarga, rollback watchdog    [validación en C puro]
  sound/              melodías, latido, volumen (lv_timer en la tarea LVGL)
  ui/                 screens: splash, home, settings, onboarding; widgets: baby, halo, status_bar; theme; i18n
  assets/             imágenes y fuentes generadas (LVGL 9)
  demo_mode/          botón + escenarios
  scenarios/          tabla de escenarios (demo y sim)
  sim_server/         CONFIG_INCUTWIN_SIM
test_apps/            app Unity on-target: twin_model, backoff, commands, ota validation, status payload, rssi levels, settings defaults
tools/                gen_assets.py, gen_extra.py, factory_provision.py, pair.py, unpair.py, release.py
```

Regla de dependencias: `ui`/`sound` dependen solo de `app_events`, `twin_model` (tipos),
`settings`, `assets`, `board`; nunca de `mqtt_link`/`net_wifi`. `twin_model` no depende de nada
de red ni de LVGL. `board` es el único que incluye `driver/*` con pines.

### D12. Consola serie mínima
`esp_console` por UART con comandos `info` (serie, client id, incubator_id, slot, fw, ip),
`usage`, `unpair` (borra `incubator_id` local; el retenido lo volverá a emparejar), `reboot`,
`factory-reset`. Sin comandos que impriman secretos.

## Risks / Trade-offs

- [IDF 6.0 y componentes del registro no compilan juntos] → Tarea 1 lo detecta el primer día;
  plan B: pinear IDF 5.5 LTS, sin cambios de código de aplicación.
- [PSRAM octal mal configurada → no arranca o LVGL sin memoria] → `sdkconfig.defaults` copiado
  del ejemplo oficial para WROOM-1-N16R8; verificación de heap en el bring-up (spec `board-bringup`).
- [Orientación/táctil distintos a la referencia] → pantalla de dianas del bring-up antes de
  escribir una sola pantalla de producto; los flags viven en `board` y se ajustan en un sitio.
- [LVGL 9: imágenes I8 con paleta y rendimiento de 2 imágenes 240×320] → RGB565 para las
  grandes, I8 solo para el bebé; medir fps en la tarea 1.
- [Windows: rutas largas, antivirus, COM ocupado por otra sesión] → proyecto en ruta corta
  (`C:\Users\Pablo\Documents\IncuTwin_FW_Crowpanel` ya es corta); `idf.py -p COM12`; cerrar
  monitores antes de flashear.
- [Rollback por WiFi caída tras OTA] → inofensivo (vuelve al anterior); se documenta y se puede
  relanzar la OTA. Timeout de 900 s configurable.
- [Payload §3 pierde `parents`/`home`/alarma] → parser tolerante con extensiones; el lado TB puede
  añadirlas sin tocar firmware. Riesgo de producto, no técnico.
- [Pérdida de la `NVS prov` antigua al cambiar particiones] → aceptado; la unidad de pruebas y la
  demo se reconfiguran por onboarding.
- [Dos 4 MB de app frente a los 6,4 MB actuales] → el binario estimado es < 2 MB; si creciera,
  cambiar la tabla exige reflasheo USB, por eso se decide ahora y se vigila con `idf.py size` en
  `release.py`.

## Migration Plan

1. Rama `feat/idf-port` desde `dev`. `dev` sigue siendo el firmware Arduino flasheable para demos
   hasta el paso 6.
2. En la rama, el árbol nuevo convive con `src/`/`platformio.ini` solo hasta la tarea 2 (sirven
   de referencia); se eliminan en la tarea 3 para que el build IDF no los vea.
3. Unidad COM12: `idf.py -p COM12 erase-flash` una vez (cambio de particiones), después
   `idf.py flash` normal. La NVS de fábrica se graba con `factory_provision.py --no-broker` hasta
   tener credenciales reales, y con alta en broker en cuanto Pablo facilite `MQTT_ADMIN_PASS` o dé
   de alta el cliente a mano.
4. Cada tarea termina con verificación en COM12 y un commit `feat(hmi): ...` (Conventional
   Commits, autor Pablo).
5. Merge `--no-ff` a `dev` cuando `broker-link`, `twin-state-ingest`, `pairing` y `device-status`
   estén verificados contra el broker real; `ota-update` puede entrar en la misma fusión o en una
   segunda. README y docs se reescriben en el stage de docs del mismo change.
6. Reflasheo del panel de demo ITW-DEEF3C y alta en el broker. `openspec archive idf-port`.

Rollback del plan: `dev` intacto hasta el merge; la rama se puede abandonar sin coste.

## Open Questions

- Formato exacto del client id: propuesto `incutwin-<YYWW-NNNN>` (p. ej. `incutwin-2640-0007`);
  el contrato pone `incutwin-0001`. Lo decide Pablo con infra antes de la tarea 7.
- Hosting del binario para probar OTA: ¿`fw.medicalopenworld.org` ya existe? Si no,
  `INCUTWIN_OTA_ALLOW_ANY_HOST` en una build dev contra un servidor local HTTPS con certificado
  público (p. ej. túnel) o contra GitHub Releases.
- ¿Añadir `thermo`/`baby`/`name` al payload de ThingsBoard (extensiones de `twin-state-ingest`)?
  Decisión de producto para el lado TB; el firmware ya las acepta.
- GT911 en otros lotes: ¿hay unidades? Si sí, `board` detecta por I2C (0x38 vs 0x5D) y carga el
  driver correspondiente (componente `esp_lcd_touch_gt911`).
