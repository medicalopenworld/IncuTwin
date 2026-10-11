## MODIFIED Requirements

### Requirement: Despacho por nombre [test-unity]
El panel SHALL extraer `<nombre>` del topic y despacharlo a: `pair` (ver `pairing`), `ota` (ver
`ota-update`), `reboot`, `test_melody`, `brightness` y `diag` (ver `device-diagnostics`). Un
nombre desconocido, un topic con niveles extra (`cmd/ota/x`) o un payload no vacío que no sea JSON
válido SHALL ignorarse con log WARN. Payloads > 1024 bytes SHALL descartarse sin parsear.

#### Scenario: Comando desconocido
- **WHEN** llega `incutwin/<id>/cmd/selfdestruct`
- **THEN** no pasa nada y el log avisa

#### Scenario: Niveles de más
- **WHEN** llega `incutwin/<id>/cmd/ota/extra`
- **THEN** se ignora

#### Scenario: diag con payload vacío
- **WHEN** llega `incutwin/<id>/cmd/diag` sin payload
- **THEN** se despacha a diagnóstico sin borrado de core dump

#### Scenario: diag con flag de borrado
- **WHEN** llega `{"erase_coredump":true}`
- **THEN** se despacha a diagnóstico con borrado; con `false`, ausente o no booleano, sin borrado

### Requirement: Origen unicast o flota [test-unity]
El despachador SHALL distinguir si el comando llegó por el topic propio o por `all`. Por `all`:
`pair` se ignora; `ota`, `reboot` y `diag` aplican un retardo aleatorio (ver cada comando);
`brightness` y `test_melody` se ejecutan de inmediato. Por `all`, el flag `erase_coredump` de
`diag` SHALL ignorarse con log WARN.

#### Scenario: Reboot de flota
- **WHEN** llega `incutwin/all/cmd/reboot`
- **THEN** el panel reinicia tras un retardo aleatorio uniforme entre 0 y 60 s (log con el valor)

#### Scenario: Diag de flota
- **WHEN** llega `incutwin/all/cmd/diag` con `{"erase_coredump":true}`
- **THEN** el informe se publica tras un retardo aleatorio uniforme entre 0 y 60 s y el core dump
  no se borra
