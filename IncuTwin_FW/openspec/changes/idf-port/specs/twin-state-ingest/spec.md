# twin-state-ingest

Cómo el estado publicado por ThingsBoard en `incubators/<id>/state` se convierte en lo que ve y
oye la familia. Payload de referencia (`docs/BROKER-MQTT-CONTEXT.md` §3):

```json
{ "incubator_id": "353", "ts": 1759320000, "state": "baby",
  "treatments": ["heat", "phototherapy", "pulseox"], "bpm": 142,
  "last_seen": 1759319940, "event_seq": 1287, "last_event": "treatment_changed" }
```

## ADDED Requirements

### Requirement: Validación del mensaje [test-unity]
Un mensaje SHALL descartarse (log a nivel WARN, sin cambiar el modelo) si: el payload supera
1024 bytes; no es un objeto JSON; `incubator_id` falta o no coincide con el emparejado; `state`
falta o no es uno de `free`, `baby`, `offline`. Los campos desconocidos SHALL ignorarse.

#### Scenario: Incubadora equivocada
- **WHEN** el panel está emparejado con `353` y llega un estado con `"incubator_id":"354"`
- **THEN** el modelo no cambia y el log avisa

#### Scenario: JSON roto
- **WHEN** llega `{"incubator_id":"353","state":"baby",`
- **THEN** el modelo no cambia y el log avisa

### Requirement: Mapeo al modelo del gemelo [test-unity]
Un mensaje válido SHALL mapearse así:

| Campo | Modelo |
|---|---|
| `state = offline` | `online = false` (el resto de campos se aplica igualmente) |
| `state = free` | `online = true`, `baby = none` |
| `state = baby` | `online = true`, `baby = in` |
| `treatments` contiene `heat` | `thermo = stable`; si no, `thermo = off` |
| `treatments` contiene `phototherapy` | `photo = true`; si no, `false` |
| `treatments` contiene `pulseox` y `bpm` es entero 1..300 | `bpm` = valor; en cualquier otro caso (`null`, ausente, fuera de rango, sin `pulseox`) `bpm = 0` |
| `treatments` ausente o vacío | `thermo = off`, `photo = false`, `bpm = 0` |

Tras aplicar, `state_rx = true`.

#### Scenario: Bebé con calor y pulso
- **WHEN** llega `state: baby, treatments: ["heat","pulseox"], bpm: 142`
- **THEN** `baby = in`, `thermo = stable`, `photo = false`, `bpm = 142`

#### Scenario: bpm sin pulsioxímetro
- **WHEN** llega `treatments: ["heat"], bpm: 142`
- **THEN** `bpm = 0` (el valor sin tratamiento activo no es fiable)

#### Scenario: Incubadora apagada
- **WHEN** llega `state: offline`
- **THEN** `online = false` y la pantalla muestra "IncuNest apagada" con la vista vacía

### Requirement: Claves opcionales de extensión [test-unity]
Para que ThingsBoard pueda enriquecer el estado sin cambiar firmware, el panel SHALL aceptar,
solo si están presentes y son válidas, estas claves adicionales, que prevalecen sobre el mapeo
anterior:

| Clave | Tipo / valores | Efecto |
|---|---|---|
| `thermo` | `off`, `heating`, `stable`, `alarm` | sustituye el `thermo` derivado de `heat` |
| `baby` | `none`, `in`, `parents`, `home` | sustituye el `baby` derivado de `state` |
| `awake` | bool | `awake` (por defecto false) |
| `skin` | entero 0..5 | tono de piel (por defecto el de Kconfig, 1) |
| `name` | string ≤ 23 bytes UTF-8 | nombre en la barra ("" = ninguno) |
| `weight_g` | entero 0..9999 | peso (0 = no mostrar) |
| `age_d` | entero 0..9999 | días (ausente = no mostrar) |

Valores inválidos de una clave opcional SHALL ignorarse sin invalidar el mensaje.

#### Scenario: Canguro vía extensión
- **WHEN** llega `state: baby, baby: "parents"`
- **THEN** `baby = parents` y la pantalla muestra "Con sus papás"

#### Scenario: Extensión inválida
- **WHEN** llega `skin: 9`
- **THEN** el tono no cambia y el resto del mensaje se aplica

### Requirement: Deduplicación por event_seq [test-unity]
El panel SHALL recordar el último `event_seq` aplicado de la incubadora emparejada (en RAM y en
NVS `twin/seq`). Un mensaje con `event_seq` ≤ al recordado SHALL aplicarse como **refresco**: se
actualiza el modelo pero no se generan transiciones (y por tanto no suena nada). Un mensaje con
`event_seq` mayor, o sin `event_seq`, SHALL aplicarse como **nuevo**. El `event_seq` SHALL
persistirse en NVS cuando `last_event` ≠ `heartbeat` o cuando el mensaje produzca una transición
de `baby`; junto a él SHALL persistirse el `baby` resultante (`twin/baby`). Al emparejar con otra
incubadora ambos SHALL ponerse a cero.

#### Scenario: Reinicio con bebé dentro
- **WHEN** el panel se reinicia tras haber aplicado `event_seq = 1287` con `baby = in`, y al
  suscribirse recibe el retenido con `event_seq = 1290`, `last_event = heartbeat`, `state = baby`
- **THEN** el modelo arranca con `baby = in` (persistido), el mensaje se aplica sin transición de
  `baby` y no suena "Bebé detectado"

#### Scenario: Evento perdido durante un apagado
- **WHEN** el panel persistió `event_seq = 1290`, `baby = in` y al reconectar recibe
  `event_seq = 1300`, `state = free`, `last_event = baby_out`
- **THEN** el modelo pasa a `baby = none` como transición (sin melodía: `baby_out` no suena) y
  persiste 1300/`none`

#### Scenario: Duplicado
- **WHEN** llega dos veces el mismo mensaje con `event_seq = 1300`
- **THEN** la segunda vez es un refresco y no cambia nada observable

### Requirement: Transiciones que suenan [test-unity]
Al aplicar un mensaje **nuevo**, el panel SHALL comparar el `baby` anterior con el resultante y
emitir: `BABY_IN` si pasa a `in` desde cualquier otro valor; `BABY_PARENTS` si pasa a `parents`
o `home` desde otro valor. `BABY_IN` SHALL disparar la melodía "Bebé detectado" y la ventana de
latido audible; `BABY_PARENTS`, la fanfarria. Ninguna otra transición SHALL sonar (en particular
`baby_out`, `treatment_changed`, `incubator_offline/online`, `heartbeat`).

#### Scenario: Llegada del bebé
- **WHEN** el modelo tenía `baby = none` y llega nuevo `state: baby, last_event: baby_in`
- **THEN** suena "Bebé detectado"

#### Scenario: Alta sin ruido
- **WHEN** el modelo tenía `baby = in` y llega nuevo `state: free, last_event: baby_out`
- **THEN** no suena nada y la vista pasa a incubadora vacía

### Requirement: Sin caducidad local de datos [test-unity]
El panel NO SHALL marcar la incubadora como apagada por ausencia de mensajes: el `offline` lo
decide ThingsBoard. La única caducidad local es la del enlace con el broker (`broker_lost`).

#### Scenario: Incubadora sin pulsioxímetro
- **WHEN** el último estado es `baby` sin `bpm` y no llega nada durante 2 h con el broker
  conectado
- **THEN** el bebé sigue en pantalla

### Requirement: Estado al desemparejar [test-unity]
Al recibir un desemparejado, el modelo SHALL volver a `paired = false`, `state_rx = false`,
`baby = none`, `bpm = 0`, tratamientos apagados y `seq = 0`.

#### Scenario: Desemparejado
- **WHEN** llega `cmd/pair` con payload vacío
- **THEN** la pantalla pasa a "Sin IncuNest vinculada" y el bebé desaparece sin sonido
