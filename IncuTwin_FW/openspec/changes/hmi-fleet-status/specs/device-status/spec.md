## MODIFIED Requirements

### Requirement: Payload de estado [test-unity]
El panel SHALL publicar en `incutwin/<client_id>/status`, retenido, QoS 1, un objeto JSON plano
con exactamente estas claves y en este orden:
`online` (siempre `true`), `fw` (versión), `rssi` (dBm entero), `incubator_id` (id o cadena
vacía), `ts` (segundos Unix o 0), `uptime_s` (segundos desde el arranque), `on_s`, `conn_s`,
`hand_s`, `hand_n` (contadores de `usage-stats`, valores en memoria), `reset` (motivo del último
reinicio: `poweron`, `ext`, `sw`, `panic`, `int_wdt`, `task_wdt`, `wdt`, `brownout`, `usb`,
`jtag` u `other`), `heap_min` (mínimo de heap interno libre desde el arranque, bytes) y
`coredump` (`true` si hay un volcado en la partición `coredump`). El payload SHALL ocupar
≤ 320 bytes; si no cabe, no se publica y se registra un error.

#### Scenario: Panel emparejado con hora
- **WHEN** fw 2.1.0, RSSI −61, `incubator_id = 353`, hora válida, 90 s encendido, contadores
  3600/1200/40/7, último reinicio por `esp_restart`, heap mínimo 150000, sin core dump
- **THEN** el payload es
  `{"online":true,"fw":"2.1.0","rssi":-61,"incubator_id":"353","ts":17593...,"uptime_s":90,"on_s":3600,"conn_s":1200,"hand_s":40,"hand_n":7,"reset":"sw","heap_min":150000,"coredump":false}`

#### Scenario: Panel sin emparejar sin hora tras un pánico
- **WHEN** no hay `incubator_id` ni hora SNTP, el último reinicio fue por excepción y hay volcado
- **THEN** el payload lleva `"incubator_id":""`, `"ts":0`, `"reset":"panic"` y `"coredump":true`

#### Scenario: Peor caso de tamaño
- **WHEN** versión de 15 caracteres, id de 16, contadores de 10 dígitos, heap de 7 dígitos
- **THEN** el payload cabe en 320 bytes
