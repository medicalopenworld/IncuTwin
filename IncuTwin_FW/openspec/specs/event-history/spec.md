# event-history

## Purpose

Lo que vive el bebé en la IncuNest queda como avisos: se ven en una franja sobre la pantalla (ver
`twin-display`), se guardan en el panel y se listan en la pantalla de Historial. Tono cálido, sin
sustos: nunca se avisa de averías, alarmas ni de la conexión.

Vocabulario: un **aviso** es un bit de `twin_notice_t`; el **último estado en línea** es el último
estado de la IncuNest emparejada con `online = true` (los mensajes `offline` no cuentan ni lo
cambian); `heat` = `thermo` ≠ `off`.

Pendiente para versiones futuras (decidido el 24-nov-2026 y aplazado): una IncuTwin por persona con
todas sus IncuNest (emparejamiento con lista, deslizar entre IncuNest en Inicio, un sonido como
máximo cada 10 min entre todas, un interruptor por IncuNest en Ajustes); el Historial ya guarda el
id de la IncuNest de cada aviso para ello. Tampoco existe aún el historial de estancias desde
ThingsBoard.

## Requirements

### Requirement: Avisos del bebé y del tratamiento [test-unity]
Al aplicar un estado nuevo, el panel SHALL comparar el último estado en línea con el resultante y
emitir:

| Cambio | Aviso | Texto (es / en) | Buzzer |
|---|---|---|---|
| `baby` ∉ {in, parents} → in o parents | `ADMISSION` | Ha entrado un bebé en la incubadora / A baby has entered the incubator | «Bebé detectado» |
| `baby` = in → parents | `PARENTS` | El bebé está con sus papás / The baby is with their parents | fanfarria |
| `baby` ∈ {in, parents} → none o home | `EXIT` | El bebé ha salido de la incubadora / The baby has left the incubator | — |
| `heat` empieza / acaba | `HEAT_ON` / `HEAT_OFF` | Ha empezado el calor / Ha terminado el calor · Warmth has started / Warmth has ended | — |
| `photo` empieza / acaba | `PHOTO_ON` / `PHOTO_OFF` | Ha empezado la fototerapia / Ha terminado la fototerapia · Phototherapy has started / Phototherapy has ended | — |

`parents` → `in`, la caída o vuelta de la conexión, un `alarm` de un ThingsBoard antiguo (se lee como calor) con calor ya activo,
pulsioxímetro, nombre y sexo NO SHALL generar aviso. Varios cambios en un mismo mensaje SHALL dar
varios avisos a la vez.

#### Scenario: Recorrido completo
- **WHEN** llegan, con `event_seq` crecientes, ingreso, fototerapia, canguro (`parents`), vuelta (`in`)
  y alta (`home`)
- **THEN** salen los avisos ADMISSION, PHOTO_ON, PARENTS y EXIT (la vuelta a `in` no da ninguno) y solo
  suenan dos melodías: ingreso y con sus papás

#### Scenario: Alarma antigua con calor
- **WHEN** `thermo` llega como `alarm` con calor ya activo
- **THEN** no hay aviso (se lee como calor y el calor sigue activo)

### Requirement: Sin avisos falsos [test-unity]
Los avisos SHALL calcularse contra el último estado en línea. Un mensaje con `event_seq` ≤ el último
no avisa (refresco). Un mensaje `offline` no avisa ni cambia la base: al volver en línea con el mismo
estado no hay aviso, y si vuelve sin bebé hay un único `EXIT`. Al arrancar, el primer mensaje SHALL
poder dar solo avisos del bebé (su estado se guarda en NVS `twin/baby`), nunca de calor ni de
fototerapia. Al cambiar de IncuNest emparejada, y en el modo acompañar, el nuevo estado SHALL tomarse
como base sin avisar.

#### Scenario: Reinicio con bebé y calor
- **WHEN** el panel se reinicia con bebé dentro y calor, y recibe el retenido
- **THEN** no hay ningún aviso al arrancar ni sonido de arranque

#### Scenario: Desconexión con bebé
- **WHEN** la IncuNest pasa a `offline` con bebé y luego vuelve en línea con el mismo estado
- **THEN** no hay aviso; si vuelve sin bebé, un único aviso `EXIT`

### Requirement: Historial persistente [manual]
El panel SHALL guardar los **50** últimos avisos en un anillo en NVS (namespace `evlog`; el build sim
usa `evlogsim` y no toca `evlog`, para no mezclar avisos de prueba con los reales). Cada
entrada guarda el tipo, la hora (epoch SNTP; 0 si aún no hay hora) y el id de la IncuNest (no se
muestra). Al llenarse se pisa la más antigua; el anillo sobrevive a un reinicio y se descarta si su
tamaño no cuadra. Los avisos reales SHALL guardarse también mientras se ve la demo; los avisos de la
demo, no. Cada bit de un aviso múltiple es una entrada.

#### Scenario: 51 avisos
- **WHEN** se generan 51 avisos
- **THEN** el Historial tiene 50 entradas y falta la más antigua

#### Scenario: Reinicio
- **WHEN** el panel se reinicia con 12 avisos guardados
- **THEN** el Historial sigue con los 12

#### Scenario: Aviso durante la demo
- **WHEN** llega un aviso real mientras se ve la demo
- **THEN** queda en el Historial; los de los escenarios de la demo no

### Requirement: Pantalla de Historial [manual]
La pantalla de Historial (botón del pollito en la barra de navegación) SHALL listar los avisos del
más reciente al más antiguo; cada fila con icono, texto del aviso en el idioma activo y tiempo
relativo («ahora», «hace 5 min», «hace 2 h», «hace 3 d»; «5 min ago»… en inglés; «-» si la entrada no
tiene hora válida). Vacía SHALL mostrar el pollito y «Aquí verás lo que vaya pasando en tu IncuNest» /
«Here you'll see what happens in your IncuNest». Los tiempos se refrescan cada minuto mientras se ve
y la lista al cambiar el historial.

El pollito de la barra SHALL llevar un punto mientras haya avisos sin ver; los avisos se marcan
vistos al abrir el Historial.

#### Scenario: Punto de sin ver
- **WHEN** llega un aviso con otra pantalla activa
- **THEN** el pollito muestra el punto; al abrir el Historial desaparece

#### Scenario: Sin hora
- **WHEN** un aviso se guardó antes de sincronizar la hora
- **THEN** su fila muestra «-» como tiempo
