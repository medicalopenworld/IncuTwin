## ADDED Requirements

### Requirement: Historial de estancias por panel [test-unity]
ThingsBoard SHALL mantener en el server attribute `itw_history` de cada panel IncuTwin la lista de
estancias que ha acompañado, de la más reciente a la más antigua, con como mucho 10. Un panel
acompaña una estancia si estaba emparejado (`MirroredBy`) con la IncuNest en algún momento de
ella. Cada entrada SHALL tener: `id` (`stay_id`), `c` (país ISO alfa-2 o `""`), `n` (nombre o
`""`), `from` y `to` (fecha local `YYYY-MM-DD`; `to = null` si sigue), `end` (`null`, `"home"` o
`"left"`), `days` (días de la estancia), `holds` ("Coge mi mano" de este panel en la estancia) y
`my_days` (días distintos con al menos uno). La lógica SHALL estar en `tb/itw_history.js`, sin
E/S, y probarse con Node en `tb/test_history.js`.

#### Scenario: Llega un bebé
- **WHEN** la IncuNest 353, emparejada con el panel, emite `baby_in` con `stay_id = 353:17`
- **THEN** el panel tiene una entrada nueva `353:17` al principio, con `from` = hoy, `to = null`,
  `end = null` y `holds = 0`

#### Scenario: Vuelve del canguro
- **WHEN** llega `baby_back` de una estancia que ya está en el historial
- **THEN** no se crea otra entrada y la existente sigue abierta

#### Scenario: Se va a casa
- **WHEN** llega `baby_out` con `outcome = "home"` y `duration_days = 18`
- **THEN** la entrada se cierra con `to` = día del evento, `end = "home"` y `days = 18`

#### Scenario: Se va sin desenlace bueno
- **WHEN** llega `baby_out` con `outcome = null` (cualquier `reason`)
- **THEN** la entrada se cierra con `end = "left"` y su nombre se borra

#### Scenario: Más de diez estancias
- **WHEN** el panel tiene 10 estancias y empieza la undécima
- **THEN** se descarta la más antigua

### Requirement: "Coge mi mano" en el historial [test-unity]
Cada "Coge mi mano" aceptado por `IncuTwin_hand_hold` SHALL sumar 1 a `holds` de la estancia en
curso del panel que lo hizo y anotar el día local. Uno rechazado no cuenta. Si la estancia aún no
está en el historial (panel emparejado antes del despliegue), SHALL crearse con `from` tomado de
`in_since` del estado de la IncuNest.

#### Scenario: Dos en el mismo día
- **WHEN** el panel completa dos "Coge mi mano" el mismo día
- **THEN** `holds` sube en 2 y `my_days` en 1

#### Scenario: Rechazado
- **WHEN** la IncuNest no tiene bebé y el aviso se rechaza
- **THEN** el historial no cambia

### Requirement: Logros calculados en el servidor [test-unity]
`itw_history.stats` SHALL tener:

| Clave | Significado |
|---|---|
| `days` | días locales distintos en que el panel completó un "Coge mi mano" aceptado, en todas sus estancias |
| `streak` | días seguidos con al menos uno, terminando hoy o ayer; 0 si el último fue antes de ayer |
| `families` | otras familias que acompañan ahora a la IncuNest de la estancia en curso: `itw_panel_count − 1`, mínimo 0; 0 sin estancia en curso |

El día local SHALL calcularse con el server attribute `itw_utc_offset_min` del panel (60 por
defecto).

#### Scenario: Racha
- **WHEN** el panel completó "Coge mi mano" el lunes, el martes y el miércoles, y hoy es miércoles
- **THEN** `streak = 3`; si hoy fuera jueves sin ninguno, seguiría en 3; el viernes, 0

#### Scenario: Familias
- **WHEN** la IncuNest de la estancia en curso tiene `itw_panel_count = 13`
- **THEN** `families = 12`

### Requirement: Entrega del historial al panel [manual]
Cada vez que `itw_history` cambie, ThingsBoard SHALL publicarlo en
`incutwin/<client_id>/cmd/history`, retenido, QoS 1, con un JSON de como mucho 1000 bytes (si no
cabe, se quitan del mensaje las estancias más antiguas; el atributo las conserva). El mensaje
SHALL llevar `v = 1`, `upd` (segundos Unix), `stats` y `stays`, y nunca `hold_days` ni datos
clínicos.

#### Scenario: Panel apagado
- **WHEN** un bebé se va a casa con el panel apagado y el panel se enciende al día siguiente
- **THEN** al conectar recibe el historial retenido con la estancia cerrada como `"home"`

#### Scenario: Tamaño
- **WHEN** las 10 estancias con nombre no caben en 1000 bytes
- **THEN** el mensaje lleva las más recientes que caben y el atributo sigue teniendo 10

### Requirement: Privacidad del historial [test-unity]
El historial SHALL llevar el nombre del bebé solo si la IncuNest tiene `itw_show_name = 'true'`, y
SHALL conservarlo tras cerrar la estancia solo si `end = "home"`. NUNCA SHALL llevar peso, semanas
de gestación, causa ni tipo de alta distinto de "a casa", temperaturas, SpO2, alarmas ni el
`baby_outcome` numérico.

#### Scenario: Sin consentimiento
- **WHEN** la IncuNest no tiene `itw_show_name = 'true'`
- **THEN** `n = ""` en todas sus estancias

#### Scenario: Fallecimiento
- **WHEN** una estancia se cierra con `baby_outcome = 2`
- **THEN** la entrada queda con `end = "left"`, sin nombre, igual que un traslado o un alta sin
  dato

### Requirement: Emparejamiento también en ThingsBoard [manual]
`tools/pair.py` SHALL, además de lo que hace en el broker, crear la relación
`IncuNest —MirroredBy→ panel` en ThingsBoard (borrando la de otra IncuNest si la había),
recalcular `itw_panel_count` en las IncuNest afectadas y disparar una sincronización del
historial del panel. `tools/unpair.py` SHALL borrar la relación, recalcular el contador y
sincronizar. Sin credenciales de ThingsBoard SHALL avisar y hacer solo la parte del broker.

#### Scenario: Emparejar con un bebé dentro
- **WHEN** se empareja un panel nuevo con una IncuNest que tiene un bebé desde hace 5 días
- **THEN** el panel recibe un historial con esa estancia abierta (`days = 5`, `holds = 0`) y la
  otra familia ve `families` subir en 1

### Requirement: País de la IncuNest [manual]
El server attribute `country` (ISO 3166-1 alfa-2) de la IncuNest SHALL viajar en las entradas del
historial (`c`) y en el estado `incubators/<id>/state` (`country`). Sin atributo, `c = ""` y no se
envía `country`.

#### Scenario: Camerún
- **WHEN** la IncuNest 353 tiene `country = "CM"`
- **THEN** su estado lleva `"country":"CM"` y sus estancias `"c":"CM"`
