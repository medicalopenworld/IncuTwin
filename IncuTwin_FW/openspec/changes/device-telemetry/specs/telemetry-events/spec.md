## ADDED Requirements

### Requirement: Lote de eventos [test-unity]
El panel SHALL publicar sus eventos en `incutwin/<id>/ev` (no retenido, QoS 1) como
`{"gen":<gen>,"b":<arranque>,"e":[{"s":<seq>,"t":<epoch o 0>,"up":<s desde arranque>,"k":"<tipo>","d":{…}},…]}`,
con como mucho 20 eventos y 4096 bytes por lote, en orden de `s`.

#### Scenario: Dos eventos
- **WHEN** un gesto de 12 s en la 353 suma un corazón (saldo 24) con `seq` 1031 y 1032
- **THEN** el lote es `{"gen":1,"b":42,"e":[{"s":1031,…,"k":"gesture","d":{"sec":12,"inc":"353","super":0}},{"s":1032,…,"k":"heart","d":{"n":1,"why":"gesture","bal":24}}]}`

#### Scenario: Lote lleno
- **WHEN** hay 45 eventos pendientes
- **THEN** salen en lotes de 20, 20 y 5, cada uno tras el PUBACK del anterior

### Requirement: Cuándo se envía [manual]
El panel SHALL enviar un lote al conectar con el broker, 60 s después del primer evento sin enviar
o al llegar a 20 pendientes, lo que antes ocurra, con un solo lote en vuelo.

#### Scenario: Gesto suelto
- **WHEN** hay un gesto y nada más durante un minuto
- **THEN** sale un lote con sus eventos a los 60 s

### Requirement: Cola persistente [test-unity]
Cada evento SHALL guardarse en la partición `storage` antes de enviarse y SHALL salir de la cola
solo tras el PUBACK de su lote. Con la cola llena (≥ 5.000 eventos) SHALL descartarse el más viejo y
sumarse a `lost`. Un reinicio no SHALL perder eventos guardados ni repetir números de orden.

#### Scenario: Sin WiFi tres días
- **WHEN** el panel genera 300 eventos sin conexión y se reinicia dos veces
- **THEN** al conectar salen los 300, en orden y sin huecos de `seq`

#### Scenario: Cola llena
- **WHEN** se generan 5.010 eventos sin conexión
- **THEN** salen los 5.000 más recientes y `lost` vale 10

#### Scenario: PUBACK perdido
- **WHEN** se cae la sesión tras publicar un lote y antes del PUBACK
- **THEN** el mismo lote (mismos `seq`) vuelve a salir al reconectar

### Requirement: Identidad de los eventos [test-unity]
`seq` SHALL subir de uno en uno durante toda la vida del panel, también tras un restablecimiento de
fábrica. `gen` SHALL subir en uno con cada restablecimiento de fábrica. El arranque `b` SHALL subir
en uno con cada arranque.

#### Scenario: Cambio de dueño
- **WHEN** el panel con `gen` 1 y `seq` 900 se restablece de fábrica
- **THEN** el siguiente evento lleva `gen` 2 y `seq` ≥ 901

### Requirement: Demo [manual]
Con la demo activa SHALL no registrarse ningún evento salvo `demo` al entrar (`{"ev":"on"}`) y al
salir (`{"ev":"off","dur_s":<duración>}`).

#### Scenario: Gesto en la demo
- **WHEN** se hace un gesto en la demo
- **THEN** no hay evento `gesture` ni `heart`
