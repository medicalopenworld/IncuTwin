# IncuTwin FW — CrowPanel Advance 2.8"

Gemelo digital de la incubadora **IncuNest** sobre el
[Elecrow CrowPanel Advance 2.8"](https://github.com/Elecrow-RD/CrowPanel-Advance-2.8-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-320x240)
(ESP32-S3-WROOM-1-N16R8, pantalla IPS ST7789 320×240, táctil FT5x06 o GT911 según lote),
montado **en vertical** (240×320, USB arriba).

Un bebé animado (respira, bosteza, duerme, late su corazón) refleja el estado real de la
incubadora. La UI está pensada para familias, niños y personas mayores: sin números, solo
estados con colores e iconos grandes, en español e inglés.

> **¿Empiezas en el proyecto?** Lee primero [docs/TRASPASO.md](docs/TRASPASO.md): qué está hecho,
> qué falta, qué secretos pedir y cómo está montado el banco de pruebas.

## Arquitectura

```
IncuNest ──telemetría──▶ ThingsBoard ──estado (retenido)──▶ Mosquitto ◀──MQTT/TLS── IncuTwin
                         (rule chains, tb/)   incubators/<id>/state       (este firmware)
```

- El panel **solo habla con el broker MQTT** (`mqtts://mqtt.medicalopenworld.org:8883`), nunca
  con ThingsBoard. Contrato completo de topics, payloads y roles:
  [docs/BROKER-MQTT-CONTEXT.md](docs/BROKER-MQTT-CONTEXT.md).
- Se suscribe al estado retenido de su incubadora (`incubators/<id>/state`), publica su
  `status` en `incutwin/<client_id>/status` y atiende comandos en `incutwin/<client_id>/cmd/#`
  (`pair`, `ota`, `reboot`, `test_melody`, `brightness`) y en el topic de flota
  `incutwin/all/cmd/#`.
- Firmware en **ESP-IDF 6.0.1** puro (sin Arduino), LVGL 9 sobre `esp_lcd` + `esp_lvgl_port`.
- Requisitos de producto en [`openspec/specs/`](openspec/specs/) (una spec por capacidad) y
  cambios en curso en [`openspec/changes/`](openspec/changes/).

## Características

- **Bebé animado** con 6 tonos de piel (intercambio de paleta), halo de color según el estado
  (calma, calentando, fototerapia, alarma) y corazón que late al ritmo real.
- **Barra de estado** con prioridad: sin WiFi, conectando, sin conexión con el servidor
  (≥ 10 min sin broker), sin IncuNest vinculada, IncuNest apagada, sin bebé, durmiendo,
  despierto, padres, alarma…
- **Sonido** por el buzzer (GPIO8): melodía de arranque, avisos y latido al tocar al bebé;
  volumen en 4 niveles.
- **Ajustes**: idioma ES/EN, volumen, brillo y reset de fábrica (con diálogo).
- **Onboarding**: idioma → QR para conectar el móvil a un SoftAP con portal cautivo que
  configura **solo el WiFi**. El emparejamiento con la incubadora es remoto (`cmd/pair`).
- **OTA por HTTPS** con verificación sha256 antes de activar el slot y **rollback** del
  bootloader si la imagen nueva no consigue conectar al broker.
- **Modo demo con el botón BOOT** (sin red): 2 s entra/sale, pulsación corta pasa al siguiente
  de los 10 escenarios (dormido, despierto, calentando, alarma, fototerapia, padres, en casa,
  vacía, apagada, sin vincular). Muestra una insignia **DEMO**.
- **Consola serie** (115200): `info`, `usage`, `wifi <ssid> <pass>`, `unpair`, `reboot`.

## Compilar y flashear

Requisitos: [ESP-IDF 6.0.1](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/get-started/)
y Python. Las dependencias (LVGL, esp-mqtt, cJSON, drivers táctiles) las descarga `idf.py` del
registro de componentes la primera vez (`main/idf_component.yml`).

```powershell
. C:\esp\v6.0.1\esp-idf\export.ps1      # o export.sh en Linux/macOS
cd IncuTwin_FW
idf.py build
idf.py -p COM12 flash monitor
```

> Si el puerto no aparece, mantén pulsado **BOOT** y pulsa **RESET** para entrar en modo
> descarga. La tabla de particiones (`ota_0` + `ota_1`, 4 MB cada uno) no se cambia por OTA:
> una placa que venga del firmware antiguo hay que flashearla por USB (mejor con `erase-flash`).

### Variantes de build

Cada variante combina `sdkconfig.defaults` con un fichero extra y usa su propio directorio:

| Build | Para qué | Comando |
|---|---|---|
| Producto | Lo que se distribuye | `idf.py build` |
| `dev` | Banco de pruebas: OTA desde cualquier host HTTPS, watchdog de validación 120 s. **No distribuir** | `idf.py -B build-dev -D SDKCONFIG=build-dev/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.dev" build` |
| `sim` | Sin MQTT ni OTA; servidor web en el panel para controlar el estado desde el móvil. **No distribuir** | `idf.py -B build-sim -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.sim" build` |
| `bringup` | Pantalla de dianas para comprobar pantalla y táctil | `idf.py -B build-bringup -D SDKCONFIG=build-bringup/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.bringup" build` |
| `rollbacktest` | Imagen que nunca conecta, para probar el rollback | ver cabecera de `sdkconfig.defaults.rollbacktest` |

### Tests

Tests Unity on-target en `test_apps/` (lógica pura: modelo del gemelo, parser, status, OTA…):

```powershell
idf.py -C test_apps -p COM12 flash
python tools/serial_capture.py --port COM12 --seconds 20 --reset
```

## Herramientas (`tools/`)

Leen las credenciales de `tools/factory/secrets/` (fuera del repo; ver
[docs/TRASPASO.md](docs/TRASPASO.md)) y nunca las imprimen.

| Script | Qué hace |
|---|---|
| `factory_provision.py` | Serializa un panel: serie `ITW-YYWW-NNNN`, alta en el broker, NVS de fábrica, etiqueta QR |
| `pair.py` / `unpair.py` | Empareja o desempareja un panel con una incubadora (rol + `cmd/pair` retenido) |
| `mqtt_cmd.py` | Manda comandos (`reboot`, `brightness`…), lee `status` o escucha un panel. **Ojo:** `--state` *publica* un estado, no lo lee |
| `release.py` | Build de release, `dist/` con sha256 y JSON del comando `ota`, subida y publicación |
| `broker_bootstrap.py` | Crea roles y clientes del contrato en el broker (idempotente) |
| `serial_capture.py` | Captura el log serie sin monitor interactivo (`--reset`, `--send`) |
| `gen_assets.py`, `gen_extra.py`, `gen_fonts.py` | Regeneran imágenes y fuentes de `components/assets` |
| `tb_api.py`, `demo_viewer.py`, `../tb/` | ThingsBoard: rule chains del estado del gemelo y visor de demo |

## Estructura

```
IncuTwin_FW/
├── main/                  # app_main.c + idf_component.yml (dependencias)
├── components/
│   ├── board/             # ST7789, táctil autodetectado, backlight, buzzer, indev LVGL propio
│   ├── ui/                # pantallas (home, ajustes, onboarding), bebé, i18n, tema
│   ├── twin_model/        # estado del gemelo: parseo, deduplicación por event_seq, derivados
│   ├── mqtt_link/         # cliente MQTT/TLS, backoff, suscripciones
│   ├── commands/          # pair, ota, reboot, test_melody, brightness
│   ├── cloud_status/      # publicación de status
│   ├── ota_update/        # OTA HTTPS + sha256 + rollback
│   ├── net_wifi/, captive_portal/, identity/, settings/, storage/
│   ├── sound/, demo_mode/, scenarios/, usage_stats/, sim_server/
│   └── app_config/        # Kconfig del producto (también lo ve test_apps)
├── test_apps/             # tests Unity on-target
├── tools/                 # fabricación, broker, release, assets
├── tb/                    # rule chains de ThingsBoard (estado del gemelo)
├── openspec/              # specs de producto y cambios en curso
├── docs/                  # contrato del broker, provisión, demo, traspaso
├── partitions.csv         # ota_0/ota_1 4 MB + otadata + coredump
└── src/, include/, platformio.ini   # firmware Arduino ANTIGUO: solo referencia, pendiente de borrar
```
