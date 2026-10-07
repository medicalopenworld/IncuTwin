# device-status

Lo único que el panel publica sobre sí mismo. Pensado para escalar a decenas de miles de unidades
(`docs/BROKER-MQTT-CONTEXT.md` §3 y §9).

## ADDED Requirements

### Requirement: Payload de estado [test-unity]
El panel SHALL publicar en `incutwin/<client_id>/status`, retenido, QoS 1, el objeto JSON
`{"online":true,"fw":"<versión>","rssi":<RSSI dBm entero>,"incubator_id":"<id o cadena vacía>","ts":<segundos Unix o 0>}`
sin otros campos y en ≤ 160 bytes.

#### Scenario: Panel emparejado con hora
- **WHEN** fw 2.0.0, RSSI −61, `incubator_id = 353`, hora válida
- **THEN** el payload es `{"online":true,"fw":"2.0.0","rssi":-61,"incubator_id":"353","ts":17593...}`

#### Scenario: Panel sin emparejar sin hora
- **WHEN** no hay `incubator_id` ni hora SNTP
- **THEN** el payload lleva `"incubator_id":""` y `"ts":0`

### Requirement: Cadencia [test-unity]
El panel SHALL publicar `status` inmediatamente tras cada CONNACK correcto, después cada 3600 s
mientras siga conectado, y además una vez tras cambiar el emparejamiento. No SHALL publicarlo en
ningún otro momento.

#### Scenario: Sesión de tres horas
- **WHEN** el panel permanece conectado 3 h sin cambios de emparejamiento
- **THEN** publica `status` 4 veces (conexión + 3 horarias)

#### Scenario: Reconexiones frecuentes
- **WHEN** la sesión cae y se recupera 5 veces en una hora
- **THEN** publica `status` 5 veces (una por reconexión) y el contador horario se reinicia con cada
  conexión

### Requirement: Coherencia con el LWT [manual]
El LWT SHALL usar el mismo topic y la misma bandera de retención que `status` (`{"online":false}`,
ver `broker-link`), de modo que el último valor retenido refleja siempre si el panel está en línea.

#### Scenario: Apagar y encender
- **WHEN** se apaga el panel 2 min y se vuelve a encender
- **THEN** un suscriptor ve `{"online":false}` tras ≤ 90 s del apagado y `{"online":true,...}` al
  reconectar
