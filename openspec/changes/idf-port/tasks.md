# idf-port — Tareas

Cortes verticales. Cada uno termina verificado en la IncuTwin de COM12 (WiFi `in3wifi`) y con un
commit propio. No se empieza el siguiente sin cerrar la verificación del anterior. Referencias:
specs en `specs/`, decisiones en `design.md`.

## 0. Preparación

- [ ] 0.1 Crear rama `feat/idf-port` desde `dev` (la crea Pablo con `/git-feature-start idf-port`)
- [x] 0.2 Esqueleto IDF: `CMakeLists.txt`, `main/`, `version.txt` (2.0.0), `sdkconfig.defaults`
      (S3, 16 MB QIO, PSRAM octal, rollback, bundle, particiones custom, FreeRTOS 1 kHz),
      `partitions.csv` (spec `ota-update`), `Kconfig.projbuild` con todas las opciones de D10
- [x] 0.3 `idf_component.yml`: lvgl ^9.6, esp_lvgl_port ^2.9, esp_lcd_touch_ft5x06 ^1.1, gt911 ^1.2,
      mqtt ^1.1 y cjson ^1.7 (fuera del árbol del IDF en 6.0); `idf.py set-target esp32s3 &&
      idf.py build` compila (1956 pasos, 204 KB)
- [x] 0.4 `.gitignore` para `build*/`, `sdkconfig`, `managed_components/`, `dist/`,
      `tools/factory/secrets/`; `idf.py -p COM12 erase-flash` (unidad ITW-DEEF3C)
- [x] 0.5 Verificado 1-oct-2026: bootloader IDF 6.0.1, tabla con `ota_0` arrancando en estado
      `valid`, QIO 80 MHz, 16 MB, PSRAM octal 8 MB, heap interno 375 KB
      (`tools/serial_capture.py --port COM12 --reset`)

## 1. Bring-up de placa (spec `board-bringup`)

- [x] 1.1 Componente `board`: pines, SPI2 + `esp_lcd_new_panel_st7789`, DMA, `invert_color`,
      mirror/swap según D2; backlight LEDC (timer 0, 5 kHz, 8 bits); buzzer LEDC (timer 1,
      10 bits); botón GPIO0; informe PSRAM/heap
- [x] 1.2 Táctil `esp_lcd_touch_ft5x06` por I2C (SDA 15, SCL 16, INT 47, 400 kHz), con
      autodetección FT5x06/GT911 por sondeo del bus
- [x] 1.3 `esp_lvgl_port` con LVGL 9.6, doble buffer parcial 240×64 en RAM interna, tick por
      esp_timer; `lv_conf` vía Kconfig de LVGL (color 16 bits, swap_bytes en el port, QR)
- [x] 1.4 Pantalla de bring-up (`CONFIG_INCUTWIN_BRINGUP_SCREEN`): rectángulos de orientación,
      texto "USB ↑", 5 dianas táctiles con marcador y eco por log, rampa de brillo, escala del
      zumbador, ciclado de combos del táctil con BOOT, fps (perf monitor)
- [x] 1.5 Verificado por Pablo el 1-oct-2026 en COM12: orientación, color, movimiento fluido,
      brillo, zumbador y las 5 dianas; táctil necesitó espejo Y (combo 6 = espejo X+Y);
      flags finales en `components/board/README.md`
- [ ] 1.6 Commit `feat(hmi): bring-up esp-idf del CrowPanel 2.8 (esp_lcd, táctil, lvgl 9)`

## 2. Assets, modelo y UI sin red (specs `twin-display`, `settings`, `sound`, `demo-mode`, `usage-stats`)

- [x] 2.1 `tools/gen_assets.py`, `gen_extra.py` (LVGL 9: I8 + paletas, ARGB8888, RGB565) y
      `gen_fonts.py` (Montserrat Medium + FA5 solid; las fuentes origen ya no van en el repo de
      LVGL); componente `assets`
- [x] 2.2 `twin_types` (solo cabeceras), `app_events` (loop propio) y `twin_model`
      (`twin_logic.c` puro + `twin_parse.c` cJSON + `twin_model.c` pegamento con overlay demo)
- [x] 2.3 `storage`, `identity` (sn, fallback MAC, credenciales, prov), `settings`, `usage_stats`
- [x] 2.4 `ui`: tema, i18n, splash, home (tres vistas, 14 prioridades, iconos, halo, WiFi,
      DEMO), `baby_widget` (LVGL 9, paletas en PSRAM), ajustes (idioma, volumen, brillo, tonos,
      pie, factory reset 1 s con msgbox)
- [x] 2.5 `sound` sobre `lv_timer` 15 ms con peticiones atómicas desde otras tareas
- [x] 2.6 `scenarios` y `demo_mode` (esp_timer 10 ms, antirrebote 30 ms, larga 2 s)
- [x] 2.7 `test_apps/` Unity on-target (status 14 filas, derive, apply/mapeo, dedup, transiciones,
      RSSI, ids, parser): 22/22 PASS en COM12 el 1-oct-2026 (`idf.py -C test_apps -p COM12 flash`
      + `serial_capture.py --reset`; el componente de tests necesita `WHOLE_ARCHIVE`)
- [ ] 2.8 Verificar en COM12: splash + melodía de arranque, los 10 escenarios con BOOT (vistas,
      halo, iconos, barra, sonidos), ajustes (idioma, volumen con pitido, brillo, reset con diálogo),
      toque del bebé con latido, insignia DEMO, usage por consola
- [ ] 2.9 Eliminar `src/`, `include/`, `platformio.ini`, `.pio/`, `.vscode/` del árbol de la rama
- [ ] 2.10 Commit(s) `feat(hmi): ui del gemelo, sonido, ajustes y demo en lvgl 9`

## 3. WiFi, portal y onboarding (specs `wifi-connectivity`, `captive-portal`, `onboarding`, `identity-provisioning`)

- [x] 3.1 `net_wifi`: STA con `prov/ssid|pass`, backoff 1→30 s, TWIN_EVT_WIFI con IP y RSSI cada
      1 s, SNTP (`CONFIG_INCUTWIN_SNTP_SERVER`), hora válida ≥ 2025 (escrito; verificación en 3.7)
- [x] 3.2 `captive_portal`: AP+STA, escaneo (≤ 15 SSID), SoftAP `IncuTwin-XXXX`/`incutwin`, DNS
      UDP comodín, `esp_http_server` con `/`, `/save`, 302 comodín; páginas ES/EN; parada limpia
- [x] 3.3 `ui/ui_onboarding.c`: pasos 1–5 según spec (QR WIFI, 25 s WiFi, 30 s servidor o salto
      sin credenciales, serie en texto y QR, Terminar → `prov/done`)
- [x] 3.4 `identity` con credenciales y "sin serializar"; factory reset selectivo en `storage`;
      consola `support_console` (`info`, `usage`, `unpair`, `reboot`, `factory-reset`) — ojo: un
      componente llamado `console` tapa al del IDF
- [x] 3.5 `tools/factory_provision.py` reescrito: client id `incutwin-<serie sin ITW->`, NVS
      `factory` + `mqtt`, alta con `mosquitto_ctrl` (o `--no-broker`), secretos en
      `tools/factory/secrets/`; falta grabar la unidad COM12
- [ ] 3.6 `test_apps`: validación de `/save`, niveles RSSI, namespaces borrados por el reset
- [ ] 3.7 Verificar en COM12: onboarding completo con el móvil contra `in3wifi` (incluida una
      contraseña errónea primero), IP por log, icono de cobertura, router apagado/encendido,
      factory reset → onboarding de nuevo conservando el serie
- [ ] 3.8 Commit `feat(hmi): wifi, portal cautivo solo-wifi y onboarding en esp-idf`

## 4. Broker: enlace, estado, emparejado, status (specs `broker-link`, `twin-state-ingest`, `pairing`, `device-status`, `device-commands`)

- [x] 4.1 `mqtt_link`: esp-mqtt TLS con bundle, LWT, suscripciones al conectar, backoff
      exponencial propio (`backoff.c`, C puro), TWIN_EVT_MQTT/MQTT_MSG, arranque con la WiFi
      (escrito; verificación en 4.8)
- [x] 4.2 `twin_model`: parser cJSON con extensiones, dedup + persistencia `twin/seq|baby`,
      transiciones, `broker_lost` a 600 s en el tick; reinicio al desemparejar
- [x] 4.3 `pairing` (en `commands`): validación de id, NVS, TWIN_EVT_PAIRING_CHANGED →
      resuscripción en `mqtt_link`, reset de seq en el modelo, `status`
- [x] 4.4 `cloud_status`: `status_payload.c` puro + cadencia (conexión, periodo Kconfig, emparejado)
- [x] 4.5 `commands`: despacho unicast/all, estado → `twin_model_ingest`, `reboot` (500 ms /
      0–60 s), `test_melody`, `brightness`; `ota` como gancho débil hasta la tarea 5
- [ ] 4.6 `test_apps`: +topics, pair, brightness, melody, backoff (secuencia, reinicio, jitter),
      payload status ≤ 160 B — escritos, pendiente de resultado en COM12
- [x] 4.7 Hecho el 2-oct-2026 con `MQTT_ADMIN_PASS`: `tools/broker_bootstrap.py` creó los roles
      `incutwin`, `thingsboard`, `publisher` y el cliente `publisher` (contraseña en `broker.env`);
      `factory_provision.py --sn ITW-DEEF3C` dio de alta `incutwin-DEEF3C` y grabó la NVS. **Hallazgo
      para infra**: la sustitución `%c`/`%u` de dynsec no funciona en este broker (SUBACK error y
      publicaciones descartadas) → cada panel lleva además un rol literal `panel-<client_id>`.
      Herramientas en Python puro con paho-mqtt (`broker_mqtt.py`, `mqtt_cmd.py`): sin mosquitto_ctrl
- [ ] 4.8 Verificado en COM12 contra el broker real (2-oct-2026): conexión TLS en 6 s desde el
      arranque ✅, `status` retenido con `ts` SNTP ✅, `cmd/pair` → suscripción + `status` con
      `incubator_id` ✅, estado `baby_in` nuevo ✅, duplicado (seq menor) como refresco ✅,
      `baby_out` nuevo ✅, `cmd/ota` ✅, consola `info` ✅, `cmd/brightness` (40, clamp 10,
      inválido) ✅, `cmd/test_melody` ✅, comando desconocido ignorado ✅, cambio 353→354 con
      resuscripción ✅, desemparejado por retenido vacío ✅, **desemparejado detectado por SUBACK
      rechazado** ✅ (nuevo requisito en `pairing`), `cmd/reboot` ✅, reconexión tras la expulsión
      de dynsec al cambiar roles (< 1 s) ✅. Pendiente: LWT (desenchufar 90 s) y caída del broker
      < / ≥ 10 min (cortar internet) — los hace Pablo; melodía/vista en el panel ✅ (Pablo)
- [ ] 4.9 Commit(s) `feat(hmi): cliente mqtt del broker, estado del gemelo, pair y status`

## 5. OTA con rollback (spec `ota-update`)

- [x] 5.1 `ota_update`: `ota_validate.c` puro (URL https + sufijo, sha256 hex), un job, retardo
      de flota con sustitución por unicast, `ota_task` con `esp_http_client` streaming +
      `esp_ota_write` + sha256 **PSA** (Mbed TLS 4 no trae `mbedtls/sha256.h`), abort/activar,
      reinicio a 1 s, progreso cada 10 % (escrito; verificación en 5.5)
- [x] 5.2 Rollback: pendiente de verificar al arrancar → watchdog Kconfig → mark invalid +
      reboot; mark valid al primer TWIN_EVT_MQTT conectado
- [x] 5.3 `tools/release.py` (build, dist, sha256, json, mosquitto_pub impreso, `--upload`,
      `--publish --target`)
- [ ] 5.4 `test_apps/test_ota.c`: URL permitida/rechazada, hash hex, comando completo — escrito,
      pendiente de resultado en COM12
- [ ] 5.5 Verificar en COM12: **OTA correcta ✅ 2-oct-2026** (build dev `ALLOW_ANY_HOST`, binario
      2.0.1 por túnel trycloudflare: 2,1 MB en 17 s, sha256 OK, reinicio a `ota_1` en
      `pending-verify`, conexión en 5,6 s, `status` con fw 2.0.1, `app marked valid`). Necesitó
      `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_CROSS_SIGNED_VERIFY=y` (cadena GTS cross-firmada; también
      protege a Let's Encrypt). También ✅ 2-oct-2026: hash incorrecto (descarga completa,
      `sha256 mismatch`, sin reinicio); corte a mitad (servidor que cierra a 600 KB → "error de
      red", sin reinicio); build que no conecta (2.0.2 con broker inexistente) → instalada,
      `pending-verify`, watchdog 120 s → rollback automático a `ota_0` 2.0.0; flasheo USB no
      pendiente. Nota: un túnel Cloudflare almacena el binario entero, así que "matar el origen"
      no corta la descarga. Imágenes de prueba en el scratchpad de la sesión (no en el repo)
- [ ] 5.6 Commit `feat(hmi): ota https con sha256 y rollback del bootloader`

## 6. Build de simulación y herramientas (specs `sim-server`, `fleet-tools`)

- [x] 6.1 `sim_server` bajo `CONFIG_INCUTWIN_SIM` (`sdkconfig.defaults.sim`, `build-sim/`):
      página, `/state`, `/incubator` (parser real con `incubator_id` "SIM"), `/demo`; sin MQTT ni
      OTA; `twin_model_sim_*` solo existen en esta build (escrito; verificación en 6.3)
- [x] 6.2 `tools/pair.py`, `tools/unpair.py`, `tools/broker_common.py` (mosquitto_ctrl +
      mosquitto_pub, roles `incubator-<id>`, secretos de `broker.env`)
- [ ] 6.3 Verificar: build sim en COM12 controlada desde el móvil; `pair.py`/`unpair.py` contra el
      cliente de pruebas
- [ ] 6.4 Commit `feat(tools): sim server, pair/unpair y release para el broker`

## 7. Documentación y cierre

- [ ] 7.1 Reescribir `README.md` (build con idf.py, estructura, demo, sim, fabricación), mover
      `docs/BROKER-MQTT-CONTEXT.md` → `docs/BROKER.md`, `docs/PROVISIONING.md` sin ThingsBoard,
      archivar `docs/THINGSBOARD_PLAN.md` y `tb/` en `docs/archive/`, `docs/SECURE_BOOT.md`
      (procedimiento de fabricación, no activado), `CLAUDE.md` raíz que apunte a `docs/BROKER.md`
      y a `openspec/`
- [ ] 7.2 Revisión (`code-reviewer`, `security-reviewer`): secretos fuera del log, validación de
      todo lo que entra por MQTT/HTTP, tareas sin trabajo largo en callbacks
- [ ] 7.3 Merge `--no-ff` a `dev`; reflashear ITW-DEEF3C; `openspec archive idf-port`
