# idf-port — Reescritura del firmware IncuTwin en ESP-IDF puro

## Why

El firmware actual (Arduino sobre PlatformIO, IDF 4.4 congelado) habla directamente con
ThingsBoard, sin TLS, y no puede cumplir tres decisiones de producto ya cerradas: conectar
**solo al broker Mosquitto** (`docs/BROKER-MQTT-CONTEXT.md`), OTA con **rollback** verificado y
**Secure Boot v2 / cifrado de flash** antes de fabricar (exigen un bootloader propio que Arduino
precompilado no permite). Como la capa de red se tira entera y el bootloader hay que controlarlo,
el momento de reescribir con arquitectura ESP-IDF es ahora, antes de la beta de 50 unidades, no
después de tener firmware probado en campo.

## What Changes

- **BREAKING** Nuevo árbol de proyecto ESP-IDF 6.0.1 (`idf.py` nativo): `main/`, `components/`,
  `sdkconfig.defaults`, `partitions.csv`, `Kconfig.projbuild`. Desaparecen `platformio.ini`,
  `src/`, `include/` y toda dependencia Arduino (LovyanGFX, PubSubClient, ArduinoJson,
  Preferences, WebServer). El código Arduino es solo referencia de comportamiento.
- **BREAKING** Conectividad: cliente MQTT `esp-mqtt` contra `mqtts://mqtt.medicalopenworld.org:8883`
  (TLS con bundle de certificados, SNI, LWT, QoS 1, backoff exponencial). Se elimina todo lo de
  ThingsBoard: provisión, shared attributes, telemetría de uso a TB, OTA por chunks MQTT,
  `secrets.h`.
- Estado del gemelo desde `incubators/<id>/state` (payload §3 del contrato) con deduplicación por
  `event_seq`; emparejamiento remoto por `cmd/pair` retenido; publicación de `status` al conectar y
  cada hora; comandos `ota`, `reboot`, `test_melody`, `brightness`; topic de flota `incutwin/all/cmd/#`.
- OTA HTTPS (`esp_http_client` + `esp_ota_ops`) con verificación sha256 antes de activar el slot,
  lista blanca de host `*.medicalopenworld.org`, retardo aleatorio 0–60 min en despliegues de
  flota, y rollback del bootloader (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`): la app solo se marca
  válida tras conectar al broker; si no lo consigue en el plazo, reinicia y el bootloader vuelve al
  slot anterior.
- **BREAKING** Tabla de particiones `ota_0` + `ota_1` (4 MB cada uno) + `otadata`, sin `factory`,
  más partición de datos reservada y `coredump`. Requiere flasheo por USB una vez; no se cambia por
  OTA.
- Pantalla, bebé animado, halo, iconos, barra de estado, ajustes, melodías y modo demo se
  **reimplementan con el mismo comportamiento** (LVGL 9 sobre `esp_lcd` + `esp_lvgl_port`), con
  estos cambios de producto: la barra distingue "sin conexión con el servidor" (≥ 10 min sin
  broker) de "conectando"; aparece "sin IncuNest vinculada" cuando no hay `incubator_id`; se añade
  brillo de pantalla (local y por comando); desaparece el watchdog local de datos obsoletos
  (`DATA_STALE_S`): el estado `offline` lo decide ThingsBoard y llega retenido.
- **BREAKING** Onboarding: el portal cautivo configura **solo WiFi** (desaparecen nombre, email y
  consentimiento RGPD del dispositivo); el paso "registrando el panel" pasa a "conectando con el
  servidor"; la pantalla final muestra el número de serie (texto + QR) en vez del enlace de
  vinculación con `pair code`.
- Identidad: credenciales MQTT (`user`, `pass`) en NVS de fábrica junto al serie, grabadas por la
  herramienta de fabricación, que además da de alta el cliente en el broker.
- Herramientas: `factory_provision.py` ampliado (alta en broker + NVS `mqtt`), `pair.py`/`unpair.py`,
  `release.py` (compila, sha256, publica); generador de assets adaptado a LVGL 9.
- Se conservan sin cambio de requisitos: contadores de uso locales (ya no se publican), build de
  simulación (`CONFIG_INCUTWIN_SIM`), escenarios de demo.
- Fuera de alcance (decisiones registradas, no implementadas): publicar "Coge mi mano" al broker
  (no hay topic en el contrato; se mantiene el botón y su efecto local); Secure Boot v2 y cifrado
  de flash (se deja el proyecto preparado: bootloader propio, Kconfig, pero los eFuses no se queman).

## Capabilities

### New Capabilities

- `board-bringup`: pantalla ST7789, táctil FT5x06, retroiluminación, zumbador, botón BOOT y PSRAM
  inicializados y verificados en orientación vertical (USB arriba).
- `twin-display`: pantalla principal (vistas bebé / incubadora vacía / con sus papás), bebé
  animado, halo, iconos de estado, icono WiFi, barra de estado con prioridades, splash.
- `settings`: pantalla de ajustes (idioma, volumen, brillo, tono de piel informativo, pie de
  información) y factory reset; persistencia en NVS.
- `sound`: melodías retro (arranque, bebé detectado, con sus papás, prueba), latido audible, 4
  niveles de volumen.
- `onboarding`: asistente de primer arranque (idioma → QR SoftAP → WiFi → servidor → serie).
- `captive-portal`: SoftAP + DNS + formulario web solo-WiFi.
- `wifi-connectivity`: estación WiFi con reconexión, niveles de cobertura por RSSI, hora por SNTP.
- `identity-provisioning`: serie y credenciales MQTT en NVS de fábrica, respaldo por MAC,
  namespaces NVS y qué borra el factory reset.
- `broker-link`: conexión MQTT TLS al broker, LWT, suscripciones, backoff, detección de pérdida
  prolongada, marcado de app válida.
- `twin-state-ingest`: parseo del payload de estado, mapeo al modelo del gemelo, `event_seq`,
  tolerancia a claves opcionales, disparo de melodías por `last_event`.
- `device-status`: publicación de `incutwin/<id>/status` y LWT.
- `pairing`: `cmd/pair` retenido, cambio de suscripción, desemparejado.
- `device-commands`: despacho de `incutwin/<id>/cmd/#` e `incutwin/all/cmd/#`, validación de
  payloads, comandos `reboot`, `test_melody`, `brightness`.
- `ota-update`: descarga HTTPS, sha256, política de URL, retardo de flota, rollback, particiones.
- `demo-mode`: ciclado de escenarios con el botón BOOT, insignia DEMO.
- `sim-server`: build de desarrollo con servidor web de simulación.
- `usage-stats`: contadores de uso persistentes (encendido, conectado, "agarra mi mano").
- `fleet-tools`: scripts de fabricación, emparejado y release.

### Modified Capabilities

Ninguna: `openspec/specs/` está vacío; todo el comportamiento vigente se captura aquí como
capacidades nuevas.

## Impact

- **Repositorio**: árbol nuevo en la rama `feat/idf-port`; al fusionar se eliminan `src/`,
  `include/`, `platformio.ini`, `.pio/`, `tb/` (lado ThingsBoard obsoleto) y las secciones de
  ThingsBoard de `README.md`, `docs/PROVISIONING.md`, `docs/THINGSBOARD_PLAN.md`. Se conservan
  `Images/`, `tools/` (adaptados) y `docs/`.
- **Dispositivos ya flasheados** (demo ITW-DEEF3C y la unidad de COM12): necesitan `erase_flash` +
  flasheo USB por el cambio de particiones; pierden su token de ThingsBoard (irrelevante) y la
  WiFi configurada.
- **Infraestructura**: cliente `incutwin-<serie>` por unidad en el broker (rol `incutwin`), rol
  `incubator-<id>` por emparejamiento, `subscribePattern 'incutwin/all/cmd/#'` en el rol
  `incutwin` para OTA de flota, hosting HTTPS de binarios bajo `*.medicalopenworld.org`.
- **Lado ThingsBoard** (otro repo): rule chain que publique `incubators/<id>/state` según §5 del
  contrato. Este firmware no funciona sin ella, pero su build y su demo (modo BOOT, build sim) sí.
- **Dependencias**: ESP-IDF 6.0.1; componentes del registro `lvgl/lvgl ^9.6`,
  `espressif/esp_lvgl_port ^2.9`, `espressif/esp_lcd_touch_ft5x06 ^1.1`,
  `espressif/esp_lcd_touch_gt911 ^1.2`, `espressif/mqtt ^1.1` y `espressif/cjson ^1.7` (en IDF 6.0
  esp-mqtt y cJSON ya no están en el árbol del IDF); `esp_http_client`, `app_update`,
  `esp_crt_bundle`, `esp_http_server`, `esp_lcd`, `nvs_flash` del propio IDF.
