# Contexto: broker MQTT IncuTwin ↔ ThingsBoard ↔ firmware IncuTwin

Fichero de contexto para Claude Code. Copiarlo como `docs/BROKER.md` en el repo de IncuTwin y
en el de IncuNest/ThingsBoard, y referenciarlo desde `CLAUDE.md`. Fecha: 1 oct 2026.

## 1. Arquitectura en tres líneas

- **ThingsBoard** (edición Community, en el VPS de MOW, en Docker) es el dueño de las incubadoras
  IncuNest. Convierte su telemetría en eventos y **publica una sola vez** el estado de cada
  incubadora en el broker, en un topic retenido.
- **Mosquitto** (broker MQTT, mismo VPS, Docker, detrás de Traefik) reparte ese estado a todas las
  IncuTwins emparejadas. El abanico lo hace el broker, no ThingsBoard.
- **Las IncuTwins físicas** (ESP32 + CrowPanel 2.8") solo hablan con el broker. **Nunca con
  ThingsBoard.** No hay que meter IncuTwins en ThingsBoard ni usar el IoT Gateway para v1.

Reglas que no se discuten:

- El firmware no lleva credenciales de admin ni de ThingsBoard. Solo su propio usuario/contraseña.
- Ningún secreto en el repo: contraseñas y la clave de admin van por variables de entorno
  (`MQTT_ADMIN_PASS`, etc.) o por NVS en el dispositivo.
- ThingsBoard no termina TLS ni abre puertos nuevos: habla con el broker por la red Docker interna.

## 2. Datos del broker (los puso infraestructura; no cambiarlos desde código)

| Parámetro | Valor |
| --- | --- |
| Host público | `mqtt.medicalopenworld.org` |
| Puerto público | `8883`, TLS obligatorio, terminado en **Traefik** (certificado Let's Encrypt, renovación automática) |
| SNI | **Obligatorio.** Conectar siempre por nombre de host, nunca por IP, o el TLS falla |
| Raíces a embeber en firmware | ISRG Root X1 y la raíz nueva de Let's Encrypt (infra la llama "ISRG Root YR"; verificar el nombre exacto en letsencrypt.org/certificates). En ESP32/mbedTLS lo más simple es `esp_crt_bundle` actualizado |
| Host interno (desde el contenedor de ThingsBoard) | `mosquitto:1883`, sin TLS, red Docker interna. **No** es `127.0.0.1`. El 1883 del broker no sale a internet |
| Ojo | El `1883` público del VPS es el **transporte MQTT de ThingsBoard** (lo usan las IncuNest). No tiene nada que ver con el broker |
| Broker | Eclipse Mosquitto 2.0.22, MQTT 3.1.1 y 5.0 |
| Autenticación | Usuario + contraseña. Sin anónimos. Plugin **dynamic-security** (clientes, roles, ACL por topic, administrados por MQTT) |
| Admin | Usuario `admin`. Solo administra clientes y roles: **no** publica ni se suscribe (ni a `$SYS`). Para monitorizar, cliente aparte con `subscribePattern '$SYS/#'` |
| `%c` en las ACL | Se sustituye por el **client id**, no por el usuario. Por eso client id = usuario en todos los dispositivos |
| Backup | Diario 21:25 UTC, 7 días: usuarios, roles y mensajes retenidos |

Conexión de administración (macOS; en Linux `--capath /etc/ssl/certs`):

```bash
export MQTT_ADMIN_PASS='...'   # nunca en el repo
CTRL="mosquitto_ctrl -h mqtt.medicalopenworld.org -p 8883 --cafile /etc/ssl/cert.pem -u admin -P $MQTT_ADMIN_PASS dynsec"
$CTRL listClients               # prueba de acceso (no usar $SYS para esto)
```

## 3. Diseño de topics (decisión de producto, implementar tal cual)

| Topic | Quién publica | Quién lee | Retenido | QoS |
| --- | --- | --- | --- | --- |
| `incubators/<incubator_id>/state` | ThingsBoard | IncuTwins emparejadas a esa incubadora | **sí** | 1 |
| `incutwin/<client_id>/status` | la IncuTwin (+ LWT) | monitorización (v1: nadie) | sí | 1 |
| `incutwin/<client_id>/cmd/pair` | Pablo / Cloud Function | la IncuTwin | **sí** | 1 |
| `incutwin/<client_id>/cmd/<otros>` | Pablo / Cloud Function | la IncuTwin | no | 1 |

`<incubator_id>` = número de serie de la IncuNest tal como se llama el dispositivo en ThingsBoard
(ej. `353`). `<client_id>` = `incutwin-<serie>` (ej. `incutwin-0001`); usuario MQTT idéntico.

### Payload de `incubators/<id>/state` (JSON, ≤ 512 bytes)

```json
{
  "incubator_id": "353",
  "ts": 1759320000,
  "state": "baby",            // "free" | "baby" | "offline"
  "treatments": ["heat", "phototherapy", "pulseox"],
  "bpm": 142,                 // null si no hay pulsioximetría
  "last_seen": 1759319940,
  "event_seq": 1287,          // contador monótono; la IncuTwin ignora seq <= último visto
  "last_event": "treatment_changed"   // "baby_in" | "baby_out" | "treatment_changed" | "heartbeat" | "incubator_offline" | "incubator_online"
}
```

Reglas: ThingsBoard publica el estado **completo** en cada evento y, si hay `bpm`, como máximo
una vez cada 60 s. Retenido: una IncuTwin que arranca recibe el último estado al suscribirse,
sin pedir nada. El `event_seq` evita reaccionar dos veces (melodía) al mismo evento.

### Payload de `incutwin/<id>/status`

```json
{ "online": true, "fw": "1.0.3", "rssi": -61, "incubator_id": "353", "ts": 1759320000 }
```

LWT en el mismo topic, retenido: `{ "online": false }`.

### Comandos (`incutwin/<id>/cmd/...`)

- `cmd/pair` (retenido): `{ "incubator_id": "353" }`. La IncuTwin lo guarda en NVS, se desuscribe
  del anterior y se suscribe a `incubators/353/state`. Payload vacío = desemparejar.
- `cmd/ota`: `{ "url": "https://.../fw.bin", "sha256": "..." }`.
- `cmd/reboot`, `cmd/test_melody`, `cmd/brightness` `{ "value": 0-100 }`.

## 4. Roles en dynamic-security

```bash
# Una vez
$CTRL createRole incutwin
$CTRL addRoleACL incutwin publishClientSend 'incutwin/%c/#' allow
$CTRL addRoleACL incutwin subscribePattern 'incutwin/%c/cmd/#' allow

$CTRL createRole thingsboard
$CTRL addRoleACL thingsboard publishClientSend 'incubators/#' allow
$CTRL createClient thingsboard          # contraseña → variable de entorno de la rule chain
$CTRL addClientRole thingsboard thingsboard

$CTRL createRole publisher              # Pablo / Cloud Function: emparejar y mandar comandos
$CTRL addRoleACL publisher publishClientSend 'incutwin/+/cmd/#' allow
$CTRL addRoleACL publisher subscribePattern 'incutwin/+/status' allow
$CTRL addRoleACL publisher subscribePattern 'incubators/#' allow

# Una vez por incubadora
$CTRL createRole incubator-353
$CTRL addRoleACL incubator-353 subscribePattern 'incubators/353/state' allow

# Una vez por IncuTwin (alta en fábrica)
$CTRL createClient incutwin-0001        # pide contraseña; la misma va al NVS
$CTRL addClientRole incutwin-0001 incutwin

# Emparejar = dar el rol de la incubadora + publicar el cmd retenido
$CTRL addClientRole incutwin-0001 incubator-353
mosquitto_pub -h mqtt.medicalopenworld.org -p 8883 --cafile /etc/ssl/cert.pem \
  -u publisher -P "$MQTT_PUB_PASS" -i publisher -q 1 -r \
  -t 'incutwin/incutwin-0001/cmd/pair' -m '{"incubator_id":"353"}'

# Desemparejar
$CTRL removeClientRole incutwin-0001 incubator-353
mosquitto_pub ... -r -t 'incutwin/incutwin-0001/cmd/pair' -n
```

Tarea para Claude Code (repo IncuTwin, carpeta `tools/`): script `provision.sh <serie>` que genera
contraseña aleatoria, da de alta el cliente, imprime el QR/etiqueta y deja un `nvs.csv` para
flashear. Y `pair.sh <serie> <incubator_id>` / `unpair.sh`. Más adelante, la Cloud Function
`pairDevice` hará lo mismo por MQTT con el rol `publisher`.

## 5. Lado ThingsBoard (repo IncuNest / rule chains)

Objetivo: rule chain "IncuNest events" que termine en un nodo **MQTT** (External → "mqtt",
existe en Community) publicando en `mosquitto:1883`.

Configuración del nodo MQTT:

| Campo | Valor |
| --- | --- |
| Topic pattern | `incubators/${deviceName}/state` (verificar que `deviceName` del metadata es el serie, p. ej. `353`; si no, usar el atributo que lo contenga) |
| Host / Port | `mosquitto` / `1883` |
| Client ID | `thingsboard` |
| Credentials | Basic: usuario `thingsboard`, contraseña del cliente creado arriba |
| Enable SSL | no (red interna) |
| Retained message | **sí** (opción presente en 3.5+; si la versión instalada no la tiene, es bloqueante: actualizar ThingsBoard o poner un microservicio puente) |
| QoS | 1 |

Antes del nodo MQTT:

1. Nodo script que construye el payload del apartado 3 a partir de la telemetría entrante y de los
   atributos de servidor (`baby_present_prev`, `treatments_prev`, `event_seq`).
2. Detección por flanco: solo pasa al nodo MQTT si cambió `state` o `treatments`, o si han
   pasado ≥ 60 s desde el último `heartbeat` publicado con `bpm` presente.
3. Eventos de inactividad/actividad del device profile (timeout 30 min) → `state: "offline"` /
   vuelta al estado real.
4. Cada publicación incrementa `event_seq` (atributo de servidor) y guarda el evento como
   telemetría `event` de la incubadora.

Lo que hay que descubrir en el repo antes de escribir la rule chain: qué claves de telemetría
publica hoy la IncuNest, cómo detecta entrada y alta del bebé, con qué nombre está cada
dispositivo en ThingsBoard, y qué versión de ThingsBoard corre en el VPS (pedir a infra).

No hacer: nodos "REST API call" al broker, suscribir ThingsBoard al broker, IoT Gateway. El
webhook a Firebase es otra rama de la misma rule chain y se hace después.

## 6. Lado firmware IncuTwin (repo IncuTwin, ESP32)

Cliente MQTT con `esp-mqtt` (ESP-IDF) o la capa equivalente del framework que use el repo:

- URI `mqtts://mqtt.medicalopenworld.org:8883`. Usando el hostname en la URI, mbedTLS envía SNI.
  Verificación de certificado con `esp_crt_bundle_attach` (o los dos PEM raíz embebidos).
- `client_id` = `username` = `incutwin-<serie>`; contraseña desde NVS (namespace `mqtt`,
  claves `user`, `pass`, `incubator_id`). Se graban en fábrica con `provision.sh`.
- Keepalive 60 s, `clean_session` true, reconexión con backoff exponencial (1 s → 60 s).
- LWT: topic `incutwin/<id>/status`, `{"online":false}`, retenido, QoS 1.
- Al conectar: publicar `status` (`online: true`, fw, rssi), suscribirse a `incutwin/<id>/cmd/#`
  QoS 1 y, si hay `incubator_id` en NVS, a `incubators/<incubator_id>/state` QoS 1.
- Al recibir `state`: si `event_seq` ≤ último visto, solo refrescar pantalla; si es mayor, aplicar
  estado (iluminación, latido a `bpm`, tratamientos) y reproducir melodía según `last_event`.
  Guardar `event_seq` en NVS para sobrevivir reinicios sin repetir melodías.
- Al recibir `cmd/pair`: actualizar NVS, cambiar suscripción. Payload vacío = desemparejar y
  mostrar "sin incubadora".
- Portal captivo + QR solo configura el WiFi (SSID/clave). El QR de la caja lleva el serie; la
  contraseña MQTT no se enseña al usuario.
- OTA (`esp_https_ota`) desde la primera versión, disparada por `cmd/ota`, con verificación sha256.
- Si la IncuTwin lleva ≥ 10 min sin conexión al broker, mostrarlo en pantalla; el estado retenido
  la pone al día al reconectar.

## 7. Prueba de extremo a extremo (sin ThingsBoard ni firmware)

```bash
# Terminal 1: simular una IncuTwin emparejada a la 353
mosquitto_sub -h mqtt.medicalopenworld.org -p 8883 --cafile /etc/ssl/cert.pem \
  -u incutwin-0001 -P "$PASS_0001" -i incutwin-0001 -q 1 -v \
  -t 'incubators/353/state' -t 'incutwin/incutwin-0001/cmd/#'

# Terminal 2: simular ThingsBoard
mosquitto_pub -h mqtt.medicalopenworld.org -p 8883 --cafile /etc/ssl/cert.pem \
  -u thingsboard -P "$PASS_TB" -i thingsboard -q 1 -r -t 'incubators/353/state' \
  -m '{"incubator_id":"353","ts":1759320000,"state":"baby","treatments":["heat"],"bpm":138,"last_seen":1759320000,"event_seq":1,"last_event":"baby_in"}'
```

Esperado: el terminal 1 recibe el mensaje; si se reconecta, lo vuelve a recibir (retenido);
`incutwin-0001` no puede publicar en `incubators/#` ni leer `incubators/354/state`.

Ya validado por infra el 1 de octubre: TLS desde internet, rechazo sin credenciales, alta/rol/ACL/baja
por admin, publicar y recibir con un cliente normal.

## 8. Pendiente / preguntas abiertas

- Versión exacta de ThingsBoard en el VPS (para la opción *Retained* del nodo MQTT).
- Nombre con el que ThingsBoard conoce a cada IncuNest (¿`353`? ¿`IncuNest-353`?).
- Quién consume `incutwin/+/status` en v1: nadie. Más adelante una Cloud Function o el IoT Gateway
  si se quiere ver el estado de las IncuTwins en ThingsBoard.
- Webhook ThingsBoard → Firebase (`ingestEvent`, HMAC): después del broker, misma rule chain.
- Pedir a infra un cliente `monitor` con `subscribePattern '$SYS/#'` cuando haga falta medir.

## 9. IncuTwins en ThingsBoard: IoT Gateway

ThingsBoard en el VPS es la **4.3.1.4** (Community). El nodo MQTT de la rule chain tiene la opción
*Retained message*, así que el apartado 5 no tiene bloqueo.

Para ver la flota de IncuTwins en ThingsBoard (activo/inactivo, `fw`, `rssi`, incubadora
emparejada, RPC de vuelta) se usa el **ThingsBoard IoT Gateway** como contenedor que se suscribe al
broker. Las IncuTwins siguen sin conectarse a ThingsBoard. Descartado: publicación directa desde
el firmware (dobla conexiones y credenciales y devuelve la carga a ThingsBoard).

Piezas:

1. Contenedor `thingsboard/tb-gateway` en la misma red Docker (lo monta infra), con acceso a
   `mosquitto:1883` y al puerto 1883 interno del contenedor de ThingsBoard, y un volumen para su
   configuración.
2. En ThingsBoard: dispositivo tipo *Gateway* llamado `incutwin-gateway`; su token va a
   `tb_gateway.json` (`host` = nombre del contenedor de ThingsBoard, `port` 1883, `accessToken`).
3. En el broker, cliente `gateway`:

```bash
$CTRL createRole gateway
$CTRL addRoleACL gateway subscribePattern 'incutwin/+/status' allow
$CTRL addRoleACL gateway publishClientSend 'incutwin/+/cmd/#' allow
$CTRL createClient gateway
$CTRL addClientRole gateway gateway
```

4. Conector MQTT del gateway (`mqtt.json`). Los nombres de campo cambian entre versiones del
   gateway: partir de la plantilla que trae la imagen y adaptar este patrón:

```json
{
  "broker": { "name": "mosquitto", "host": "mosquitto", "port": 1883, "clientId": "gateway",
              "security": { "type": "basic", "username": "gateway", "password": "${MQTT_GATEWAY_PASS}" } },
  "mapping": [{
    "topicFilter": "incutwin/+/status",
    "converter": {
      "type": "json",
      "deviceInfo": {
        "deviceNameExpressionSource": "topic",
        "deviceNameExpression": "(?<=incutwin/)(.*?)(?=/status)",
        "deviceProfileExpressionSource": "constant",
        "deviceProfileExpression": "incutwin"
      },
      "attributes": [ { "type": "string", "key": "fw", "value": "${fw}" },
                      { "type": "string", "key": "incubator_id", "value": "${incubator_id}" } ],
      "timeseries": [ { "type": "bool", "key": "online", "value": "${online}" },
                      { "type": "int", "key": "rssi", "value": "${rssi}" } ]
    }
  }],
  "requestsMapping": {
    "serverSideRpc": [{
      "deviceNameFilter": ".*",
      "methodFilter": ".*",
      "requestTopicExpression": "incutwin/${deviceName}/cmd/${methodName}",
      "valueExpression": "${params}"
    }]
  }
}
```

El gateway no toca `incubators/#`. Un RPC `ota` o `reboot` lanzado desde ThingsBoard sobre
`incutwin-0001` acaba publicado en `incutwin/incutwin-0001/cmd/ota`.

Regla de firmware para que escale: `status` **al conectar y luego una vez por hora**, no más
(50.000 unidades → 14 msg/s; cada minuto serían 830/s).

## 10. OTA de las IncuTwins

Mecanismo: `cmd/ota` por el broker (apartado 3) + `esp_https_ota` en el firmware. Tres decisiones
que **no se pueden cambiar por OTA** y van en la primera versión:

1. **Particiones**: sin `factory`; `ota_0` + `ota_1` + `otadata`. La CrowPanel 2.8" (ESP32-S3,
   8 MB de flash) admite dos slots de 3 MB. Activar `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`: el
   firmware nuevo llama a `esp_ota_mark_app_valid_cancel_rollback()` **solo después de conectar al
   broker**; si no lo hace, el bootloader vuelve al anterior en el siguiente reinicio.
2. **Firmware firmado**: Secure Boot v2 del ESP32-S3 se activa quemando eFuses en fábrica y es
   irreversible. Beta de 50: opcional. Lanzamiento: decidir antes de fabricar. Mientras tanto el
   firmware verifica el `sha256` del comando y solo acepta URLs bajo `*.medicalopenworld.org`.
3. **Hosting del binario**: directorio estático HTTPS detrás de Traefik
   (`https://fw.medicalopenworld.org/incutwin/<version>.bin`, lo monta infra) o GitHub Releases si
   el repo es público. El sistema de paquetes OTA de ThingsBoard **no sirve** para dispositivos que
   entran por el gateway.

Despliegue:

- Unidad a unidad: RPC `ota` desde ThingsBoard, o `mosquitto_pub` a `incutwin/<id>/cmd/ota` con
  `{"url": "...", "sha256": "..."}`. La IncuTwin reinicia y publica `status` con el `fw` nuevo.
- En masa: añadir al rol `incutwin` `subscribePattern 'incutwin/all/cmd/#'` y publicar una vez en
  `incutwin/all/cmd/ota`. El firmware espera un retardo aleatorio de 0 a 60 min antes de descargar.
  Fases: 5 unidades → 24 h → `all`.
- No hacer: OTA troceando el binario en mensajes MQTT.

Tareas para Claude Code (repo IncuTwin): `partitions.csv` con los dos slots, Kconfig con rollback,
módulo `ota.c` (descarga, sha256, marcar válido tras conectar), handler de `cmd/ota` y de
`incutwin/all/cmd/#`, y `tools/release.sh` que compila, calcula sha256 y sube el binario al
hosting.
