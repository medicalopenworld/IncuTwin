# Traspaso del firmware IncuTwin

Estado a **31-oct-2026**, versión **2.0.0** (tag `hmi-v2.0.0` en `main`). Resume qué está hecho,
qué falta y lo que no está escrito en el código. El detalle de cada requisito vive en
[`openspec/specs/`](../openspec/specs/) y el de las tareas en
[`openspec/changes/`](../openspec/changes/).

## 1. Qué está hecho

**Port completo a ESP-IDF 6.0.1** (cambio OpenSpec [`idf-port`](../openspec/changes/idf-port/)).
El firmware Arduino/PlatformIO se reescribió desde cero con componentes IDF y el panel pasó de
hablar con ThingsBoard a hablar **solo con el broker Mosquitto**
([contrato](BROKER-MQTT-CONTEXT.md)). Hecho y verificado en la unidad de banco:

- Bring-up de placa: ST7789, táctil autodetectado (FT5x06 0x38 / GT911 0x5D, Elecrow cambia el
  chip según lote), backlight, buzzer en GPIO8, LVGL 9.
- UI completa en LVGL 9: bebé con 6 tonos de piel, halo, iconos, barra de estado, ajustes
  (idioma, volumen, brillo, reset), onboarding, modo demo con BOOT (10 escenarios).
- WiFi, SoftAP + portal cautivo (solo WiFi) y onboarding.
- Enlace MQTT/TLS con backoff, `status` retenido, emparejamiento por `cmd/pair`, estado del
  gemelo con deduplicación por `event_seq`, comandos `ota`, `reboot`, `test_melody`,
  `brightness`, topic de flota. Verificado contra el broker real el 2-oct-2026 (detalle en la
  tarea 4.8 de `idf-port/tasks.md`).
- OTA HTTPS con sha256 y rollback: OTA correcta, hash incorrecto, corte a mitad y rollback
  automático de una imagen que no conecta, todo verificado el 2-oct-2026 (tarea 5.5).
- Tests Unity on-target en `test_apps/`: 22/22 en verde el 1-oct-2026.
- Herramientas de fabricación, broker y release en `tools/` (ver README).
- Broker: roles y clientes creados con `tools/broker_bootstrap.py`. Desde Mosquitto 2.1.2 la ACL
  `%c` funciona, así que cada panel solo necesita el rol `incutwin` más `incubator-<id>` al
  emparejarlo.
- ThingsBoard: la rule chain del estado del gemelo (`tb/itw_state_machine.js`) publica
  `incubators/<SN>/state` en el broker. Probado de punta a punta con la incubadora `TEST-incubadora`
  y demo con **IncuNest-1_2** (guion en [DEMO.md](DEMO.md)). Se despliega con
  `python tb/tb_apply_incutwin_state.py`.

## 2. Qué falta

### 2.1 Cerrar `idf-port` (prioridad alta)

El código está hecho y en `main`, pero el cambio OpenSpec no está cerrado. En `tasks.md` las
casillas de "Commit …" siguen sin marcar aunque los commits existen; lo que falta de verdad es:

- [ ] **Verificación manual de la UI** en el panel (tarea 2.8): melodía de arranque, los 10
  escenarios con BOOT, ajustes, toque del bebé con latido, insignia DEMO.
- [ ] **Onboarding con el móvil** (3.7): contraseña errónea primero, router apagado/encendido,
  reset de fábrica → onboarding conservando el serie.
- [ ] **Pruebas de red pendientes** (4.8): LWT (desenchufar 90 s) y caída del broker < 10 min y
  ≥ 10 min (cortar internet).
- [ ] **Resultado de `test_apps/test_ota.c`** en el panel (5.4) y tests de 3.6 y 4.6.
- [ ] **Build `sim`** controlada desde el móvil y `pair.py`/`unpair.py` contra el cliente de
  pruebas (6.3).
- [ ] **Borrar el firmware antiguo** (2.9): `src/`, `include/`, `platformio.ini`. Hoy solo sirven
  de referencia de comportamiento.
- [ ] **Documentación** (7.1): renombrar `BROKER-MQTT-CONTEXT.md` → `BROKER.md`, quitar
  ThingsBoard de `PROVISIONING.md`, archivar `THINGSBOARD_PLAN.md`, `FIREBASE_*.md` y `tb/` en
  `docs/archive/` si ya no aplican, escribir `docs/SECURE_BOOT.md`.
- [ ] **Revisión de seguridad** (7.2): secretos fuera del log, validación de todo lo que entra
  por MQTT/HTTP, nada largo en callbacks.
- [ ] `openspec archive idf-port` (7.3). Las specs ya están sincronizadas en `openspec/specs/`.

### 2.2 Siguiente feature: `hmi-fleet-status`

Propuesta escrita en la rama `feat/fleet-status`
([proposal](../openspec/changes/hmi-fleet-status/proposal.md)), **sin implementar**. Amplía
`status` con uptime, contadores de uso, motivo de reinicio, heap mínimo y core dump, y añade el
comando `cmd/diag` (MQTT y consola serie). Es la telemetría mínima para dar soporte a la beta.
Las tareas siguen TDD: primero los tests en `test_apps`.

### 2.3 Antes de fabricar la beta (50 unidades)

- **Hosting de OTA**: infraestructura aún no ha montado `fw.medicalopenworld.org`. El build de
  producto solo acepta OTA desde `*.medicalopenworld.org`; hasta entonces las pruebas de OTA se
  hacen con el build `dev` y un túnel `cloudflared tunnel --url http://localhost:8000`.
- **Secure Boot v2 y cifrado de flash**: era uno de los motivos del port a IDF, pero no está
  activado ni documentado. Es irreversible por placa: probar primero en una unidad sacrificable.
- **Preguntas abiertas** del contrato del broker: ver §8 de
  [BROKER-MQTT-CONTEXT.md](BROKER-MQTT-CONTEXT.md).

## 3. Secretos y accesos que hay que pedir

Ninguno está en el repo (el repo es público). Pídeselos a Pablo por un canal seguro:

| Qué | Dónde va | Para qué |
|---|---|---|
| `tools/factory/secrets/broker.env` | local, gitignored | Usuarios `publisher`, `thingsboard` y admin del broker (dynamic-security) |
| `tools/factory/secrets/<SN>.json` | local, gitignored | Contraseña MQTT de cada panel ya serializado |
| `tools/factory/secrets/thingsboard.env` | local, gitignored (plantilla en `tools/factory/thingsboard.env.example`) | Scripts de `tb/` y `tools/tb_api.py` |
| Usuario propio en ThingsBoard (`mon.medicalopenworld.org`) | — | Ver incubadoras y rule chains |

`include/secrets.h` (claves de provisión de ThingsBoard) solo lo usaba el firmware Arduino
antiguo; el firmware IDF no lo necesita.

## 4. Banco de pruebas

- Panel **ITW-DEEF3C** (CrowPanel Advance 2.8", flash 16 MB, PSRAM 8 MB), en COM12 en el puesto
  de Pablo. Serializado: cliente MQTT `incutwin-DEEF3C`, emparejado con la incubadora de prueba
  `353`. Lleva el build **`dev`**.
- WiFi del laboratorio: `in3wifi`. Se configura por consola con `wifi <ssid> <pass>` o con el
  portal cautivo.
- Captura de log sin monitor interactivo:
  `python tools/serial_capture.py --port COM12 --seconds 8 --reset`.

## 5. Trampas conocidas

- **`tools/mqtt_cmd.py --state <id>` publica, no lee.** Contra una incubadora real pisa su estado
  retenido y deja en el panel un `event_seq` falso que descarta los eventos reales. Si pasa:
  republicar el estado real (atributo `itw_state` del device en ThingsBoard) y hacer
  unpair/pair del panel para resetear el `seq`.
- **No usar `lvgl_port_add_touch()`** de `esp_lvgl_port`: aborta con un NACK transitorio del
  I2C del táctil. `components/board` crea su propio indev que trata el error como "sin toque".
- **ESP-IDF 6**: esp-mqtt y cJSON vienen del registro de componentes, no del IDF; Mbed TLS 4 solo
  tiene la API PSA (no hay `mbedtls/sha256.h`); un componente propio llamado `console` tapa el del
  IDF (por eso el nuestro es `support_console`); `test_apps` necesita `WHOLE_ARCHIVE` o no
  encuentra los tests.
- **OTA con cadenas cross-firmadas** (Google Trust Services, Let's Encrypt): hace falta
  `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_CROSS_SIGNED_VERIFY=y` (ya está en `sdkconfig.defaults`).
- **Core dump** cuando la tabla de particiones del panel no coincide con la del build:
  `esptool read-flash 0xFE0000 0x20000 cd.bin` y
  `esp-coredump info_corefile --core cd.bin --core-format raw build/incutwin.elf`.
- **Orientación**: el panel va en vertical con el USB arriba. Si la imagen o el táctil salen
  girados, se corrige con las opciones Kconfig de pantalla y táctil (`idf.py menuconfig`), que
  `components/board/board.c` traduce a `LCD_SWAP_XY`, `LCD_MIRROR_*`, `TOUCH_SWAP_XY` y
  `TOUCH_MIRROR_*`.

## 6. Forma de trabajo

- El firmware vive en `IncuTwin_FW/` dentro del monorepo (junto a `app/` y `servidor-push/`).
- Gitflow: `feat/<slug>` sale de `dev` y vuelve a `dev` con `merge --no-ff`; `main` solo recibe
  releases (`release/<x.y.z>`) y se etiqueta `hmi-v<x.y.z>`. Commits Conventional
  (`feat(hmi): …`, `docs(spec): …`).
- Cada feature empieza con una propuesta OpenSpec (`openspec/changes/<cambio>/`: proposal,
  design, specs, tasks) y se cierra con `openspec archive`.
- `.claude/` contiene agentes, skills y hooks para trabajar con Claude Code con este mismo flujo
  (loop de stages, revisión, retro). Es opcional, pero las convenciones de arriba aplican igual.
