## Context

El panel publica hoy `incutwin/<client_id>/status` con cinco campos (`cloud_status` +
`status_payload.c`, C puro con test) al conectar, cada 3600 s y al cambiar el emparejado. Los
contadores de uso viven en `usage_stats` (NVS cada 600 s) y solo se leen por consola. Los comandos
llegan por `commands` (despacho por nombre, parseo en `commands_parse.c`, C puro con test); el
reinicio de flota ya usa un `esp_timer` con retardo aleatorio. El enlace (`mqtt_link`) no cuenta
reconexiones. La partición `coredump` existe y `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y`, pero nadie
consulta si hay un volcado. `board_touch_error_count()` ya expone los errores I2C del táctil.

Restricciones: los handlers del bus de eventos y los callbacks de `esp_timer` deben ser cortos
(< 5 ms); nada de la familia sale del panel sin pedirlo; el contrato del broker (§3) fija topic,
retención y cadencia de `status`; el gateway de ThingsBoard mapea JSON plano.

## Goals / Non-Goals

**Goals:**

- Que el `status` retenido baste para saber, sin tocar el panel, si se usa, si se reinicia solo y
  si está corto de memoria.
- Un informe de diagnóstico completo a petición, por MQTT y por consola, con el mismo contenido.
- Toda la construcción de payloads en C puro testeable en `test_apps`.
- Cero cambios en UI, sonido, NVS, particiones y OTA.

**Non-Goals:**

- Subir el core dump al servidor (solo se informa de su presencia y se permite borrarlo).
- Telemetría periódica distinta de `status` (nada nuevo cada minuto).
- Publicar SSID, IP o cualquier dato de red en `status`; la IP solo va en `diag`.
- Dashboards del gateway de ThingsBoard (otro repo).

## Decisions

### D1. Componente nuevo `diagnostics` con dos capas

`diag_payload.c` (C puro: `diag_info_t` → JSON, sin IDF salvo cabeceras de tipos) y
`diagnostics.c` (recolector: `esp_system`, `esp_heap_caps`, `esp_ota_ops`, `esp_core_dump`,
`twin_model`, `usage_stats`, `settings`, `board`, `mqtt_link`, `net_wifi`). `status_payload.c`
pasa a recibir un `status_info_t` que rellena el mismo recolector. Alternativa descartada: meter
la recogida en `cloud_status`; acoplaría ese componente a media docena de módulos y `commands`
tendría que duplicarla para `diag`.

### D2. `status` plano, orden fijo, tope 320 bytes

Campos nuevos detrás de los actuales: `uptime_s`, `on_s`, `conn_s`, `hand_s`, `hand_n`, `reset`,
`heap_min`, `coredump`. JSON plano (sin objeto `usage`) porque el conversor del gateway mapea
claves de primer nivel. Motivo de reinicio como cadena corta (`poweron`, `ext`, `sw`, `panic`,
`int_wdt`, `task_wdt`, `wdt`, `brownout`, `usb`, `jtag`, `other`): legible en un dashboard y
estable aunque IDF renumere el enum. `heap_min` = `heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL
| MALLOC_CAP_8BIT)`, que es la cifra que anticipa un `malloc` fallido en la pila de red.
Alternativa descartada: `status` sin cambios y un topic `telemetry` nuevo; duplica retención y
cadencia y obliga a tocar las ACL de todos los consumidores.

### D3. Core dump: presencia cacheada al arrancar, borrado bajo demanda

`esp_core_dump_image_check()` verifica el CRC de todo el volcado (hasta 128 KB de flash): demasiado
para un handler. `diagnostics_init()` llama una vez en el arranque a `esp_core_dump_image_get()`
(solo lee la cabecera) y guarda el bool; el volcado solo cambia con un pánico (reinicio) o con el
borrado explícito, así que el valor cacheado es exacto. `{"erase_coredump":true}` llama a
`esp_core_dump_image_erase()` después de publicar el informe y pone el bool a `false`.

### D4. El trabajo de `diag` corre en una tarea efímera, no en el bus de eventos

`commands` solo valida el payload y arma un `esp_timer` one-shot (0 ms unicast, aleatorio 0–60 s
flota, igual que `reboot`). El callback crea `diag_task` (core 0, prio 3, 4 KB) que recoge, construye
el JSON en un buffer estático de 1024 B, publica con QoS 1 sin retención, borra el core dump si se
pidió y termina. Así el borrado de flash (cientos de ms) y el `esp_mqtt_client_publish` nunca
bloquean `incutwin_evt` ni el timer. Un `diag` que llega con la tarea viva se ignora con WARN
(mismo criterio que la OTA). Alternativa descartada: publicar desde el callback del timer; el
borrado de flash lo descarta.

### D5. Contador de reconexiones en `mqtt_link`

`mqtt_link_reconnect_count()` cuenta `MQTT_EVENT_CONNECTED` a partir del segundo desde el
arranque. Es la señal más barata de un WiFi o un broker inestables sin publicar nada nuevo.

### D6. `twin_model_last_seq()` en vez de ampliar el snapshot

El último `event_seq` solo interesa al diagnóstico; añadirlo a `twin_snapshot_t` lo copiaría en cada
evento de estado para nada. Un getter bajo el mismo mutex basta.

### D7. Consola

Comando serie `diag` que imprime el mismo JSON (reutiliza el constructor). `info` se queda como
está: es legible para humanos; `diag` es para pegar en un ticket.

### Modelo de tareas

| Tarea | Origen | Core | Prio | Stack | Cambio |
|---|---|---|---|---|---|
| `incutwin_evt` | esp_event | 0 | 5 | 6 KB | `commands` valida `diag` y arma el timer (< 1 ms) |
| `diag_delay` | esp_timer (cb) | — | — | — | crea `diag_task` |
| `diag_task` | nueva, bajo demanda | 0 | 3 | 4 KB | recoge, construye, publica, borra core dump, muere |
| `mqtt_task` | esp-mqtt | 0 | 5 | 8 KB | sin cambio; `reconnect_count` se incrementa en el handler |

Flujo: `cmd/diag` → `commands` → `esp_timer` → `diag_task` → `diagnostics_collect()` →
`diag_payload_build()` → `mqtt_link_publish("incutwin/<id>/diag")`. `status` sigue su ruta
(`cloud_status`) pero rellena `status_info_t` con `diagnostics_collect_status()`.

## Risks / Trade-offs

- [El `status` crece y el contrato §3 lo describe con cinco campos] → Es aditivo; el gateway y
  las herramientas ignoran claves desconocidas. Se actualiza §3 del doc del broker en el stage de
  docs.
- [`diag` de flota a 50.000 paneles: pico de ~830 msg/s de 1 KB] → Es una herramienta de soporte,
  no de rutina; el retardo aleatorio ya lo reparte en 60 s; se documenta "no usar `all` sin
  avisar a infraestructura".
- [`heap_caps_get_minimum_free_size` recorre los heaps] → Microsegundos; se mide en la verificación.
- [Un `erase_coredump` por flota borra evidencia en todos los paneles] → Solo se acepta por
  unicast; por `all` se ignora el flag con WARN.
- [El rol `publisher` no puede leer `incutwin/+/diag` hasta que infra añada la ACL] → La
  herramienta lo detecta (SUBACK fallido) y lo dice; el firmware no depende de ello.

## Migration Plan

Aditivo: una OTA normal. Los paneles con firmware anterior siguen publicando el `status` corto y
ignoran `cmd/diag` ("comando desconocido"). No hay cambio de NVS ni de particiones. Rollback:
la OTA anterior.

## Open Questions

- ¿Quiere infraestructura las claves nuevas de `status` en el gateway desde el principio o solo
  `online`/`fw`/`rssi`? No bloquea el firmware.
