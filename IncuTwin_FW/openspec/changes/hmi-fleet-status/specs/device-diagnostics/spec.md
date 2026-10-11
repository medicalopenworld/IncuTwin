# device-diagnostics

Informe de diagnóstico del panel a petición, para soporte y banco de pruebas. Complementa el
`status` retenido (`device-status`): este es completo y solo se publica cuando se pide.

## ADDED Requirements

### Requirement: Informe diag por MQTT [test-unity]
Al recibir `cmd/diag` el panel SHALL publicar en `incutwin/<client_id>/diag`, QoS 1, **no**
retenido, un objeto JSON plano de ≤ 1024 bytes con exactamente estas claves y en este orden:
`sn`, `hw`, `fw`, `idf`, `slot` (`ota_0`/`ota_1`), `ota` (`new`, `pending-verify`, `valid`,
`invalid`, `aborted`, `undefined`), `reset` (misma tabla que `device-status`), `uptime_s`,
`heap_free`, `heap_min`, `psram_free` (bytes), `wifi` (bool), `rssi`, `ip` (cadena vacía sin
WiFi), `broker` (bool, sesión activa), `reconn` (reconexiones al broker desde el arranque),
`incubator_id`, `state_rx` (bool), `seq` (último `event_seq` aplicado), `touch_err` (errores I2C
del táctil), `on_s`, `conn_s`, `hand_s`, `hand_n`, `lang` (`es`/`en`), `vol` (0–3), `bright`
(10–100), `coredump` (bool) y `ts` (segundos Unix o 0). Las cadenas SHALL escaparse como JSON;
nunca se incluyen SSID, contraseñas ni credenciales MQTT.

#### Scenario: Informe completo
- **WHEN** el panel ITW-2640-0007 (hwrev `A`), fw 2.1.0 en `ota_1` válido, reinicio `poweron`,
  120 s encendido, heap 180000/150000, PSRAM 8000000, WiFi −58 dBm IP 192.168.1.20, broker
  conectado con 2 reconexiones, emparejado con 353 y seq 1287, 3 errores táctiles, contadores
  3600/1200/40/7, es / vol 2 / brillo 80, sin core dump, ts 1759320000
- **THEN** el payload es
  `{"sn":"ITW-2640-0007","hw":"A","fw":"2.1.0","idf":"v6.0.1","slot":"ota_1","ota":"valid","reset":"poweron","uptime_s":120,"heap_free":180000,"heap_min":150000,"psram_free":8000000,"wifi":true,"rssi":-58,"ip":"192.168.1.20","broker":true,"reconn":2,"incubator_id":"353","state_rx":true,"seq":1287,"touch_err":3,"on_s":3600,"conn_s":1200,"hand_s":40,"hand_n":7,"lang":"es","vol":2,"bright":80,"coredump":false,"ts":1759320000}`

#### Scenario: Sin red ni emparejado
- **WHEN** no hay WiFi, ni broker, ni `incubator_id`
- **THEN** el payload lleva `"wifi":false`, `"rssi":-100` o el último leído, `"ip":""`,
  `"broker":false`, `"incubator_id":""`, `"state_rx":false`, `"seq":0`

#### Scenario: Peor caso de tamaño
- **WHEN** todas las cadenas tienen su longitud máxima y todos los enteros 10 dígitos
- **THEN** el payload cabe en 1024 bytes

### Requirement: Motivo de reinicio [test-unity]
El panel SHALL mapear `esp_reset_reason()` a: `POWERON→poweron`, `EXT→ext`, `SW→sw`,
`PANIC→panic`, `INT_WDT→int_wdt`, `TASK_WDT→task_wdt`, `WDT→wdt`, `BROWNOUT→brownout`,
`USB→usb`, `JTAG→jtag` y cualquier otro valor → `other`.

#### Scenario: Tabla completa
- **WHEN** se consulta cada valor del enum, incluido uno fuera de tabla
- **THEN** se obtiene la cadena indicada y `other` para el desconocido

### Requirement: Disparo [manual]
Por unicast el informe SHALL publicarse en < 2 s desde la recepción del comando. Por flota
(`incutwin/all/cmd/diag`) SHALL publicarse tras un retardo aleatorio uniforme entre 0 y 60 s. Un
`diag` recibido mientras otro está en curso SHALL ignorarse con log WARN. La recogida y la
publicación SHALL ocurrir fuera del hilo de red y del bus de eventos.

#### Scenario: Soporte pide un informe
- **WHEN** `python tools/mqtt_cmd.py --diag ITW-2640-0007`
- **THEN** la herramienta imprime el JSON en menos de 2 s

#### Scenario: Dos peticiones seguidas
- **WHEN** llegan dos `cmd/diag` con 100 ms de diferencia
- **THEN** se publica un solo informe y el log avisa del segundo

### Requirement: Core dump [manual]
`diagnostics` SHALL comprobar una sola vez en el arranque si hay un volcado válido en la partición
`coredump` y reflejarlo en `status` y `diag`. Con `{"erase_coredump":true}` por unicast, el panel
SHALL borrar el volcado **después** de publicar el informe y los siguientes `status`/`diag` SHALL
llevar `"coredump":false` sin reiniciar.

#### Scenario: Pánico forzado
- **WHEN** se provoca un pánico (comando serie de prueba o `assert`) y el panel arranca de nuevo
- **THEN** el primer `status` lleva `"reset":"panic"` y `"coredump":true`

#### Scenario: Borrado remoto
- **WHEN** llega `{"erase_coredump":true}` por unicast
- **THEN** el `diag` publicado lleva `"coredump":true`, el log confirma el borrado y un `diag`
  posterior lleva `"coredump":false`

### Requirement: Consola [manual]
El comando serie `diag` SHALL imprimir exactamente el mismo JSON que se publicaría por MQTT, con
WiFi o sin él.

#### Scenario: Banco de pruebas sin red
- **WHEN** se escribe `diag` en el monitor serie de un panel sin WiFi
- **THEN** se imprime el JSON con `"wifi":false` y `"broker":false`
