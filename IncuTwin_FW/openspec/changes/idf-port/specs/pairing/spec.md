# pairing

Emparejar un panel con una incubadora lo hace Medical Open World publicando un mensaje retenido;
el panel solo obedece. Nada de esto pasa por la pantalla.

## ADDED Requirements

### Requirement: Comando de emparejado [test-unity]
El panel SHALL, al recibir `incutwin/<client_id>/cmd/pair` con `{"incubator_id":"<id>"}` donde
`<id>` tiene 1–16 caracteres de `[A-Za-z0-9_-]`: guardar `<id>` en NVS `mqtt/incubator_id`;
si había otra incubadora, desuscribirse de su topic de estado; poner a cero `twin/seq` y
`twin/baby` y `state_rx = false`; suscribirse con QoS 1 a `incubators/<id>/state`; y publicar
`status`. Si `<id>` coincide con el ya guardado SHALL asegurarse de estar suscrito y no hacer nada
más.

#### Scenario: Primer emparejado
- **WHEN** un panel sin emparejar recibe `{"incubator_id":"353"}`
- **THEN** en < 2 s está suscrito a `incubators/353/state`, la barra pasa de "Sin IncuNest
  vinculada" a "Esperando a la IncuNest..." y, al llegar el retenido, al estado real

#### Scenario: Cambio de incubadora
- **WHEN** un panel emparejado con `353` (con `seq = 1300`) recibe `{"incubator_id":"354"}`
- **THEN** se desuscribe de `incubators/353/state`, `seq` vuelve a 0 y se suscribe a
  `incubators/354/state`; el primer estado de 354 se aplica como nuevo

#### Scenario: Mismo id repetido
- **WHEN** reconecta y recibe el retenido con el mismo `incubator_id`
- **THEN** no reinicia `seq` ni publica un `status` adicional al de la conexión

### Requirement: Desemparejado [test-unity]
Al recibir `cmd/pair` con payload vacío (0 bytes), el panel SHALL borrar `mqtt/incubator_id`,
desuscribirse del topic de estado, reiniciar el modelo del gemelo (ver `twin-state-ingest`) y
publicar `status`.

#### Scenario: Payload vacío
- **WHEN** llega `cmd/pair` sin payload
- **THEN** la pantalla muestra "Sin IncuNest vinculada" y `status` lleva `"incubator_id":""`

### Requirement: Comandos de emparejado inválidos [test-unity]
Payloads no vacíos que no sean un objeto JSON con `incubator_id` válido SHALL ignorarse con log
WARN, sin tocar el emparejamiento actual. `cmd/pair` recibido por el topic de flota
(`incutwin/all/cmd/pair`) SHALL ignorarse siempre.

#### Scenario: Id con caracteres prohibidos
- **WHEN** llega `{"incubator_id":"353/state"}`
- **THEN** el emparejamiento no cambia y el log avisa

#### Scenario: Pair por flota
- **WHEN** llega `incutwin/all/cmd/pair` con `{"incubator_id":"353"}`
- **THEN** se ignora

### Requirement: Desemparejado detectado por el broker [manual]
Si al suscribirse a `incubators/<id>/state` el broker rechaza la suscripción (SUBACK con
error), el panel SHALL interpretarlo como que ya no tiene acceso a esa incubadora: borra
`mqtt/incubator_id`, reinicia el modelo y publica `status`, igual que con un `cmd/pair` vacío.
Motivo: el retenido vacío solo llega a quien está conectado en ese instante, y Mosquitto
desconecta al cliente cuando le cambian los roles; un panel apagado durante el desemparejado
solo puede enterarse por este rechazo.

#### Scenario: Desemparejado con el panel apagado
- **WHEN** se desempareja un panel apagado (retenido vacío + retirada del rol `incubator-<id>`) y
  luego el panel arranca
- **THEN** su suscripción a `incubators/<id>/state` es rechazada y la pantalla pasa a "Sin
  IncuNest vinculada" en < 5 s desde la conexión

### Requirement: Persistencia del emparejado [test-unity]
Tras reiniciar, el panel SHALL suscribirse al estado de la incubadora guardada sin esperar a que
llegue de nuevo el `cmd/pair` retenido (que llegará igualmente y será un no-op).

#### Scenario: Reinicio emparejado
- **WHEN** el panel reinicia con `mqtt/incubator_id = 353`
- **THEN** la primera suscripción tras CONNACK incluye `incubators/353/state`
