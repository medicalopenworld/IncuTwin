# identity-provisioning

Quién es cada panel y qué sabe de sí mismo al salir de fábrica. La identidad y las credenciales
del broker se graban en fabricación y no las toca ni la familia ni el factory reset.

## ADDED Requirements

### Requirement: Namespaces NVS [test-unity]
El panel SHALL usar estos namespaces en la partición `nvs`:

| Namespace | Claves | Quién escribe | Borra el factory reset |
|---|---|---|---|
| `factory` | `sn`, `hwrev`, `batch` | fabricación | no |
| `mqtt` | `user`, `pass`, `incubator_id` | fabricación (`user`, `pass`); `cmd/pair` (`incubator_id`) | no |
| `prov` | `done`, `ssid`, `pass` | onboarding | sí |
| `settings` | `lang`, `vol`, `bright` | ajustes y comandos | sí |
| `twin` | `seq`, `baby` | ingesta de estado | sí |
| `usage` | `on`, `conn`, `hand`, `handn` | contadores de uso | no |

#### Scenario: Factory reset selectivo
- **WHEN** se ejecuta el restablecimiento de fábrica
- **THEN** `prov`, `settings` y `twin` quedan vacíos y `factory`, `mqtt` y `usage` conservan sus
  valores

### Requirement: Número de serie [test-unity]
El serie SHALL leerse de `factory/sn`. Si no existe, SHALL derivarse de la MAC WiFi eFuse como
`ITW-XXXXXX` (últimos 3 bytes en hexadecimal mayúsculas) y el log SHALL avisar "sin NVS de
fábrica".

#### Scenario: Con NVS de fábrica
- **WHEN** `factory/sn = ITW-2640-0007`
- **THEN** `identity_sn()` devuelve `ITW-2640-0007`

#### Scenario: Sin NVS de fábrica
- **WHEN** no existe `factory/sn` y la MAC termina en `DE:EF:3C`
- **THEN** el serie es `ITW-DEEF3C` y el log lo avisa una vez

### Requirement: Credenciales del broker [test-unity]
`mqtt/user` SHALL ser a la vez el nombre de usuario y el client id MQTT (`incutwin-<serie corto>`,
ver `fleet-tools`); `mqtt/pass` la contraseña. Si falta cualquiera de los dos, el panel SHALL
considerarse "sin serializar": no intenta conectar al broker, lo indica en la barra de estado y
en el log, y el resto del panel funciona.

#### Scenario: Credenciales presentes
- **WHEN** `mqtt/user = incutwin-2640-0007` y `mqtt/pass` tiene 24 caracteres
- **THEN** el cliente MQTT conecta con ese client id y usuario

#### Scenario: Panel sin serializar
- **WHEN** no existe `mqtt/pass`
- **THEN** no hay intentos de conexión al broker, la barra dice "Panel sin serializar" y el
  modo demo funciona

### Requirement: Secretos fuera del log [test-unity]
El panel NUNCA SHALL imprimir `mqtt/pass` ni `prov/pass` por el puerto serie, ni completos ni
parciales.

#### Scenario: Arranque con log verbose
- **WHEN** el nivel de log es DEBUG
- **THEN** ninguna línea contiene la contraseña WiFi ni la del broker

### Requirement: Partición NVS compatible con fabricación [test-unity]
La partición `nvs` SHALL estar en el offset 0x9000 con tamaño 0x5000, para que la imagen generada
por la herramienta de fabricación se flashee sin cambiar el firmware. Una imagen NVS de fábrica
flasheada antes o después del firmware SHALL leerse igual.

#### Scenario: Orden de flasheo
- **WHEN** se flashea primero el firmware y luego la NVS de fábrica, o al revés
- **THEN** en ambos casos el arranque muestra el serie y conecta con las credenciales grabadas
