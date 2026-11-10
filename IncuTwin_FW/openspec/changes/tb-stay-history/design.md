# Diseño — tb-stay-history

## Contexto

- La máquina de estados de la IncuNest (`tb/itw_state_machine.js`) emite `baby_in`,
  `baby_back`, `baby_parents` y `baby_out` con `stay_id = SN:baby_seq`. `baby_out` lleva
  `reason`, `duration_days` y `outcome` (`"home"` o `null`, opción B del contrato).
- "Coge mi mano" del panel entra como telemetría `{"hand_hold":1}` en el device del panel
  (creado por el IoT Gateway, nombre = client id) → IncuNest emparejada → `IncuTwin_hand_hold`,
  que lo acepta (`baby_state ∈ {in, parents}`) o lo rechaza. El panel ya va en `msg.panel`.
- ThingsBoard publica al broker con un nodo MQTT (cliente `thingsboard`, solo `incubators/#` hoy).
- El emparejamiento hoy es solo del broker (`tools/pair.py`: rol + `cmd/pair` retenido). La
  relación `MirroredBy` en ThingsBoard la crea "backend MOW" a mano.

## Decisiones

### D1. El historial vive en ThingsBoard, por panel

Server attribute `itw_history` (JSON) en el device del panel. Es la fuente de verdad y el panel
solo lo cachea. Alternativas descartadas:
- *En el panel (NVS)*: se pierde con un reset de fábrica o un cambio de panel, y el panel no
  distingue estancias.
- *En Firestore*: es de la app; los paneles no hablan con Firebase.

### D2. Un script puro y testeable

`tb/itw_history.js` es una función sin E/S: `(history, entrada, ahora) → { history, publicar }`.
Entradas:

| `kind` | Origen | Efecto |
|---|---|---|
| `stay_open` | `baby_in` / `baby_back` de la IncuNest | abre la entrada de esa estancia o la reabre |
| `stay_close` | `baby_out` | la cierra con `to`, `end` (`home` o `left`) y `days` |
| `hold` | `IncuTwin_hand_hold` aceptado | `holds++` en la estancia y apunta el día local |
| `sync` | emparejar, cambio de `itw_panel_count` o `country` | refresca familias, país y estancia en curso (la crea o reabre si la IncuNest dice que sigue) |
| `unpaired` | desemparejar | la estancia en curso queda "acompañada hasta hoy" (`to` = hoy, `end = null`) |

Se prueba con `tb/test_history.js` igual que `test_sm.js`: los mismos escenarios de la spec. En
esta spec, `[test-unity]` significa "test automático": aquí corre en Node, no en `test_apps`
(la config del proyecto solo define `[test-unity]` y `[manual]`).

### D3. Formato del atributo y del mensaje (≤ 1000 bytes)

```json
{ "v": 1, "upd": 1794260594, "tz": 60,
  "stats": { "days": 23, "streak": 5, "last": "2026-11-10", "families": 12 },
  "stays": [
    { "id": "353:17", "c": "CM", "n": "Lucía", "from": "2026-10-28", "to": null, "end": null,
      "days": 13, "holds": 9, "my_days": 6 },
    { "id": "353:12", "c": "CM", "n": "", "from": "2026-09-02", "to": "2026-09-30", "end": "home",
      "days": 28, "holds": 14, "my_days": 11 } ] }
```

- `stays`: de la más reciente a la más antigua; como mucho 10. Si el JSON pasa de 1000 bytes se
  quitan las más antiguas del **mensaje** (el atributo conserva 10).
- `end`: `null` mientras sigue; `"home"` si `outcome = "home"`; `"left"` en cualquier otro caso
  (alta sin desenlace, fallecimiento, traslado, sesión terminada, sustituida). El panel dirá
  "¡Ya está en casa!" o "Ya no está en la IncuNest". Nunca se distingue más.
- `days`: días de la estancia (`duration_days` al cerrar; desde `in_since` mientras sigue).
- `n`: nombre solo si `ss_itw_show_name = 'true'` en la IncuNest al abrir o actualizar; al cerrar
  con `end = "left"` se borra. Sin consentimiento, `""` (el panel muestra "Bebé de <país>").
- Fechas como `YYYY-MM-DD` en hora local del panel (D5); no hace falta más precisión.
- El día de cada "Coge mi mano" se guarda aparte en el atributo (`hold_days`, los últimos 60
  días distintos) para calcular `stats.days` y `stats.streak`; no va en el mensaje.

### D4. Familias que acompañan, y a qué paneles avisar

`stats.families` = `itw_panel_count − 1` de la IncuNest de la estancia en curso (0 si no hay).
ThingsBoard no cuenta relaciones desde un script, así que el contador lo mantienen
`pair.py`/`unpair.py` (D6), junto con `itw_panels` (JSON con los nombres de sus paneles).

`itw_panels` resuelve además el reparto: en ThingsBoard Community "change originator" a
relacionados se queda con el **primero**, y "duplicate to related" es de la edición PE. Así que el
evento de estancia se convierte en un array (una entrada por panel de `itw_panels`), un nodo
"split array msg" lo parte y cada copia cambia el originador al panel por nombre (`$[panel]`).
(La rama ITW_PANEL de la cadena de estado y `hand_hold_until → panel` siguen llegando solo al
primer panel: limitación previa, fuera de este cambio.) No incluye padrinos de la app (Firestore): queda para cuando la app
exista.

### D5. Qué es "un día"

El día local del panel, con un desplazamiento fijo en minutos: server attribute
`itw_utc_offset_min` en el panel, 60 por defecto (España en invierno). Sin zonas IANA porque el
motor JS de ThingsBoard no las trae. El cambio de horario de verano mueve la frontera una hora:
se acepta. La racha cuenta días seguidos hasta hoy o hasta ayer (hoy aún puedes hacerlo).

### D6. Emparejar también en ThingsBoard

`pair.py <panel> <incubadora>` además de lo que hace hoy: crea la relación `MirroredBy`
(IncuNest → panel) si no existe, borra la de otra IncuNest si la había (antes manda `unpaired`),
recalcula `itw_panels`/`itw_panel_count` de las IncuNest afectadas y pide un `sync` a todos los
paneles de la IncuNest. La petición es el server attribute `itw_history_sync`
(`{"kind":"sync"|"unpaired","panel":"ITW-…","t":ms}`) en el panel: llega a la cadena `IncuTwin`
como *Attributes Updated* (la API REST no puede mandar telemetría en nombre del panel). El
nombre del panel va dentro porque esos mensajes no traen `deviceName`. `unpair.py` hace lo inverso. Se usa `tools/tb_api.py` (lee el
`.env` de secretos; sin credenciales de ThingsBoard avisa y solo hace la parte del broker).

### D7. Publicación al broker

Nodo MQTT nuevo en `IncuTwin_history`, igual que el de estado pero con topic
`incutwin/${itw_client}/cmd/history`, retenido, QoS 1, y client id `thingsboard-history` (con el
mismo id que el nodo del estado se echarían del broker una a otra). `itw_client` lo calcula el
script a partir del nombre del panel como `tools/broker_mqtt.client_id_for`: los paneles de
fábrica están en ThingsBoard como `ITW-DEEF3C` y su client id es `incutwin-DEEF3C`. ACL: `addRoleACL thingsboard publishClientSend 'incutwin/+/cmd/history' allow`.
Retenido porque el panel puede estar apagado cuando cambia y debe recibirlo al conectar.

## Riesgos

- **Paneles sin relación `MirroredBy`**: no reciben historial. Mitigación: D6 y una comprobación
  en `tools/fleet_status` (fuera de este cambio) que liste paneles emparejados en el broker pero
  no en ThingsBoard.
- **Estancias anteriores al despliegue**: no hay historia. La estancia en curso aparece en el
  primer `sync`, `hold` o evento, con `from` sacado de `in_since` de `itw_state`.
- **Eventos con `ts` pasado** (cola del firmware de la IncuNest): `stay_close` usa el `ts` del
  evento para `to`.
- **Concurrencia**: un `hold` y un evento de estancia que lleguen casi a la vez pueden leer el
  mismo `itw_history` y pisarse (lectura de atributo → script → escritura). Es raro (un alta en el
  mismo segundo que un "Coge mi mano") y lo peor es perder un `holds++`. Al implementar se
  comprueba si la cola de ThingsBoard con estrategia `SEQUENTIAL_BY_ORIGINATOR` basta tras el
  cambio de originador; si no, se acepta el riesgo y se documenta.
