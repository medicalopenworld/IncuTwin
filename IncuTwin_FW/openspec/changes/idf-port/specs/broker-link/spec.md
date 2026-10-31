# broker-link

Sesión MQTT del panel con el broker Mosquitto. Es la única conexión saliente del producto. Datos
del broker en `docs/BROKER-MQTT-CONTEXT.md` §2 y §6.

## ADDED Requirements

### Requirement: Conexión TLS al broker [manual]
El panel SHALL conectar a `mqtts://mqtt.medicalopenworld.org:8883` (URI configurable en Kconfig,
nunca por IP) verificando el certificado del servidor con el bundle de CAs raíz embebido (que
incluye ISRG Root X1 y la raíz vigente de Let's Encrypt) y enviando SNI con el nombre de host.
Parámetros: MQTT 3.1.1, `client_id` = `username` = `mqtt/user`, contraseña `mqtt/pass`,
keepalive 60 s, sesión limpia.

#### Scenario: Conexión correcta
- **WHEN** el panel tiene WiFi con internet y credenciales válidas
- **THEN** el log muestra `mqtt: connected` en < 10 s desde la IP y el pie de ajustes dice
  "Servidor: Conectado"

#### Scenario: Certificado no confiable
- **WHEN** se apunta (en una build de desarrollo) a un broker con certificado autofirmado
- **THEN** el handshake TLS falla, el log indica error de verificación y el panel NO envía
  credenciales

#### Scenario: Credenciales rechazadas
- **WHEN** el broker devuelve CONNACK "not authorized"
- **THEN** el log lo indica con ese motivo y el panel reintenta con el backoff normal

### Requirement: Último deseo (LWT) [manual]
Al conectar, el panel SHALL registrar un LWT en `incutwin/<client_id>/status` con payload
`{"online":false}`, retenido, QoS 1.

#### Scenario: Corte de alimentación
- **WHEN** se desenchufa el panel conectado y se espera ≥ 90 s (keepalive × 1,5)
- **THEN** un suscriptor a `incutwin/<client_id>/status` recibe `{"online":false}` retenido

### Requirement: Acciones al conectar [manual]
Tras cada CONNACK correcto el panel SHALL, en este orden: publicar `status` (ver
`device-status`); suscribirse con QoS 1 a `incutwin/<client_id>/cmd/#` y a
`incutwin/all/cmd/#`; y, si hay `mqtt/incubator_id`, suscribirse con QoS 1 a
`incubators/<incubator_id>/state`. Un rechazo de suscripción (código 0x80) SHALL registrarse en el
log sin abortar la sesión.

#### Scenario: Panel emparejado reconecta
- **WHEN** reconecta con `incubator_id = 353`
- **THEN** en < 2 s recibe el `cmd/pair` retenido y el `incubators/353/state` retenido, y la
  pantalla queda al día sin intervención

#### Scenario: Topic de flota sin ACL todavía
- **WHEN** el rol del broker aún no permite `incutwin/all/cmd/#`
- **THEN** el log muestra el rechazo de esa suscripción y las demás funcionan

### Requirement: Reconexión con backoff exponencial [test-unity]
Tras una desconexión o un fallo de conexión, el panel SHALL esperar 1 s antes del primer
reintento y duplicar la espera en cada fallo consecutivo hasta un tope de 60 s, con un jitter
aleatorio de ±20 %. Una conexión correcta SHALL reiniciar la espera a 1 s. Sin WiFi no se
cuenta ni se intenta.

#### Scenario: Secuencia de esperas
- **WHEN** fallan 8 intentos seguidos
- **THEN** las esperas nominales son 1, 2, 4, 8, 16, 32, 60, 60 s (cada una ±20 %)

#### Scenario: Reinicio del backoff
- **WHEN** tras 5 fallos la conexión tiene éxito y luego se pierde
- **THEN** el siguiente reintento ocurre ~1 s después

### Requirement: Pérdida prolongada visible [test-unity]
El panel SHALL registrar el instante de la última desconexión y exponer `broker_lost` = verdadero
cuando lleve ≥ 600 s sin sesión tras haberla tenido. Mientras `broker_lost` sea falso, el estado
mostrado SHALL seguir siendo el último conocido.

#### Scenario: Umbral de 10 minutos
- **WHEN** la sesión se pierde en t = 0 y no se recupera
- **THEN** `broker_lost` es falso en t = 599 s y verdadero en t = 600 s

### Requirement: Validación de la aplicación tras OTA [test-unity]
El panel SHALL marcar la aplicación como válida inmediatamente después del primer CONNACK
correcto cuando esté pendiente de verificación (primer arranque tras una OTA). Si no logra
conectar en 900 s desde el arranque (configurable), SHALL marcarla inválida y reiniciar para que
el bootloader vuelva al firmware anterior.

#### Scenario: OTA buena
- **WHEN** arranca un firmware recién instalado y conecta al broker a los 12 s
- **THEN** el log muestra `ota: app marked valid` y el slot queda como válido tras reiniciar

#### Scenario: OTA que no conecta
- **WHEN** arranca un firmware recién instalado cuyo cliente MQTT está roto
- **THEN** a los 900 s el panel reinicia y arranca el firmware anterior (el log muestra el slot
  anterior y `rollback`)

### Requirement: Disciplina de tráfico [test-unity]
El panel NO SHALL publicar nada que no esté en esta spec, en `device-status` o en los acuses
propios del protocolo: ni telemetría periódica, ni "coge mi mano", ni respuestas a comandos.

#### Scenario: Una hora conectado sin eventos
- **WHEN** el panel pasa 1 h conectado sin comandos ni pulsaciones
- **THEN** solo se observa un PUBLISH de `status` (el de la hora) además de los PINGREQ
