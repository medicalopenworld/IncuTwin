# device-commands

Comandos que llegan por `incutwin/<client_id>/cmd/<nombre>` (unicast) e
`incutwin/all/cmd/<nombre>` (flota). Nunca se responde por MQTT.

## ADDED Requirements

### Requirement: Despacho por nombre [test-unity]
El panel SHALL extraer `<nombre>` del topic y despacharlo a: `pair` (ver `pairing`), `ota` (ver
`ota-update`), `reboot`, `test_melody`, `brightness`. Un nombre desconocido, un topic con niveles
extra (`cmd/ota/x`) o un payload no vacío que no sea JSON válido SHALL ignorarse con log WARN.
Payloads > 1024 bytes SHALL descartarse sin parsear.

#### Scenario: Comando desconocido
- **WHEN** llega `incutwin/<id>/cmd/selfdestruct`
- **THEN** no pasa nada y el log avisa

#### Scenario: Niveles de más
- **WHEN** llega `incutwin/<id>/cmd/ota/extra`
- **THEN** se ignora

### Requirement: Origen unicast o flota [test-unity]
El despachador SHALL distinguir si el comando llegó por el topic propio o por `all`. Por `all`:
`pair` se ignora; `ota` y `reboot` aplican un retardo aleatorio (ver cada comando); `brightness`
y `test_melody` se ejecutan de inmediato.

#### Scenario: Reboot de flota
- **WHEN** llega `incutwin/all/cmd/reboot`
- **THEN** el panel reinicia tras un retardo aleatorio uniforme entre 0 y 60 s (log con el valor)

### Requirement: reboot [manual]
`cmd/reboot` (payload vacío o `{}`) SHALL reiniciar el panel 500 ms después de recibirlo
(unicast) para dar tiempo al log.

#### Scenario: Reinicio remoto
- **WHEN** llega `incutwin/<id>/cmd/reboot`
- **THEN** el log muestra `cmd: reboot` y el panel arranca de nuevo mostrando el splash

### Requirement: test_melody [manual]
`cmd/test_melody` SHALL reproducir la melodía "Bebé detectado" al volumen configurado (si el
volumen es Apagado no suena nada). Payload opcional `{"melody":"boot"|"baby"|"parents"|"test"}`
para elegir otra.

#### Scenario: Prueba de altavoz
- **WHEN** llega `cmd/test_melody` con volumen Alto
- **THEN** suenan las tres notas de "Bebé detectado"

#### Scenario: Fanfarria a petición
- **WHEN** llega `{"melody":"parents"}`
- **THEN** suena la fanfarria completa

### Requirement: brightness [test-unity]
`cmd/brightness` con `{"value": n}` SHALL fijar el brillo a `clamp(n, 10, 100)` %, aplicarlo en
tiempo real y persistirlo en `settings/bright`. Sin `value` entero SHALL ignorarse.

#### Scenario: Valor en rango
- **WHEN** llega `{"value": 40}`
- **THEN** la retroiluminación pasa al 40 % y tras reiniciar sigue al 40 %

#### Scenario: Valor fuera de rango
- **WHEN** llega `{"value": 0}`
- **THEN** el brillo queda al 10 %

#### Scenario: Payload inválido
- **WHEN** llega `{"value": "alto"}`
- **THEN** el brillo no cambia y el log avisa

### Requirement: Ejecución fuera del hilo de red [test-unity]
Ningún comando SHALL bloquear la recepción MQTT: la ejecución (reinicio diferido, descarga OTA,
melodía) ocurre fuera del manejador de mensajes, que SHALL devolver el control en < 50 ms.

#### Scenario: OTA y pair seguidos
- **WHEN** llega `cmd/ota` y 100 ms después `cmd/pair`
- **THEN** el emparejado se procesa mientras la descarga sigue en curso
