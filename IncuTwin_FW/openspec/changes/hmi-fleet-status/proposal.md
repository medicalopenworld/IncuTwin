# hmi-fleet-status — Telemetría de flota en `status` y comando `diag`

## Why

La beta de 50 unidades sale con un panel que solo cuenta de sí mismo `fw`, `rssi` e
`incubator_id` una vez por hora: los contadores de uso se quedan en NVS, nadie sabe si un panel
se reinicia por watchdog o brownout, ni si tiene un core dump esperando, ni cuánta RAM le queda.
Cuando una familia diga "el panel no va" la única herramienta será pedirle que lo traiga con el
USB. Hace falta la telemetría mínima de operación antes de serializar la primera tanda, porque
el gateway de ThingsBoard (`docs/BROKER-MQTT-CONTEXT.md` §9) solo mostrará lo que el panel publique.

## What Changes

- **`status` ampliado** (sin cambiar topic, retención ni cadencia): además de los campos
  actuales, el panel publica `uptime_s`, los cuatro contadores de uso (`on_s`, `conn_s`, `hand_s`,
  `hand_n`), el motivo del último reinicio (`reset`), el mínimo de heap interno libre desde el
  arranque (`heap_min`) y si hay un core dump en flash (`coredump`). El límite de tamaño pasa de
  160 a 320 bytes. El payload sigue siendo JSON plano para que el conversor del gateway mapee
  claves sin anidar.
- **Nuevo comando `cmd/diag`**: a petición (unicast o flota, con retardo aleatorio 0–60 s en
  flota), el panel publica una foto completa de diagnóstico en `incutwin/<client_id>/diag`
  (QoS 1, no retenido, ≤ 1024 bytes): identidad, versión, slot y estado OTA, uptime y reinicio,
  memoria interna y PSRAM, WiFi, enlace con el broker (reconexiones desde el arranque),
  emparejado y último `event_seq`, errores I2C del táctil, contadores de uso, ajustes y
  core dump. Con `{"erase_coredump":true}` borra el core dump tras informar.
- **Comando serie `diag`** que imprime el mismo JSON por UART, para el banco de pruebas.
- **Contador de reconexiones** al broker desde el arranque, nuevo en el enlace MQTT.
- **Herramienta `tools/mqtt_cmd.py`**: subcomando `diag` que manda el comando y espera la
  respuesta en `incutwin/<id>/diag`.
- Los contadores de uso dejan de ser "solo locales": se publican dentro de `status`.
- Sin cambios en la UI ni en el sonido. Nada de lo publicado identifica a la familia (no se
  publica el SSID ni la IP en `status`; la IP solo va en `diag`, que se pide expresamente).

## Capabilities

### New Capabilities

- `device-diagnostics`: contenido, tamaño y disparo del informe `diag` (MQTT y consola), motivo
  de reinicio, memoria mínima, core dump y su borrado.

### Modified Capabilities

- `device-status`: el payload de `status` gana los campos de uso, uptime, reinicio, heap mínimo y
  core dump; el tope de tamaño sube a 320 bytes.
- `device-commands`: se despacha `diag` (unicast inmediato, flota con retardo aleatorio 0–60 s);
  el payload opcional `{"erase_coredump":true}` se valida como el resto.
- `usage-stats`: los contadores pasan a publicarse en `status` (se retira la frase "no se
  publican") y se exponen para el informe `diag`.

## Impact

- **Firmware**: `components/cloud_status` (payload y recogida de datos), `components/commands`
  (despacho y parseo de `diag`), nuevo `components/diagnostics` (recolector + constructor del
  JSON en C puro, testeable), `components/mqtt_link` (contador de reconexiones),
  `components/usage_stats` (sin cambio de API), `components/support_console` (comando `diag`),
  `test_apps` (payload de `status`, payload de `diag`, parseo del comando).
- **Broker**: el rol `incutwin` ya puede publicar en `incutwin/%c/#`, así que `diag` no exige ACL
  nueva en los paneles. El rol `publisher` (herramientas de Pablo) necesita
  `subscribePattern 'incutwin/+/diag'` para leer las respuestas; el gateway de ThingsBoard, si
  quiere verlas, también. Un `diag` de flota a 50.000 unidades son ≤ 1 KB × 50.000 en 60 s
  (~830 msg/s de pico): es una herramienta de soporte, no de rutina, y el retardo lo reparte.
- **ThingsBoard**: el conversor del gateway (`docs/BROKER-MQTT-CONTEXT.md` §9) puede mapear las
  claves nuevas de `status` como telemetría del panel sin cambios en el firmware.
- **Tamaño del `status`**: pasa de ~90 a ~230 bytes; a 3600 s por panel el caudal sigue siendo
  despreciable (50.000 unidades ≈ 3,2 KB/s).
- **Sin impacto** en particiones, NVS, OTA ni en la UI.
