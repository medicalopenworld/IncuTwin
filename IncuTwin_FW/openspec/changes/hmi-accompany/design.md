# hmi-accompany — Diseño

## Enfoques considerados

| Enfoque | A favor | En contra |
|---|---|---|
| **A. Elige el panel** (`incubators/+/state` + historial en NVS) — **elegido** | Solo firmware y un rol del broker; se puede flashear hoy; el panel funciona aunque ThingsBoard no sepa de él | Cada panel recibe el estado de todas las IncuNest (privacidad, tráfico ∝ nº IncuNest) |
| B. Elige el servidor (ThingsBoard asigna con `cmd/pair` y guarda el historial) | Control central, reparto, sin datos de otros bebés en casa | Rule chain nueva, el servidor necesita admin del broker para los roles; no se prueba con un flasheo |
| C. Lista de disponibles publicada por ThingsBoard + panel elige | Menos tráfico | Necesita el rol comodín igualmente y cambios en ThingsBoard |

Decisión de Claude con el visto bueno de Pablo ("adelante", 23-nov): A ahora; B o un estado sin
datos personales como fase 2.

## D1. Activación

`cmd/pair` retenido es la única fuente de "a qué estoy unido": `{"mode":"accompany"}` activa el
modo (borra `mqtt/incubator_id`, pone `mqtt/acc_mode = 1`); `{"incubator_id":...}` o vacío lo
apaga. Repetir el mismo retenido es un no-op. `mqtt/acc_mode` sobrevive al reset de fábrica, como
el emparejado; el historial (`acc`) no.

## D2. Datos

- Tabla de IncuNest vistas (máx. 64): id, `present`, `home`, y el último payload (≤ 1024 B) en
  PSRAM para poder aplicar el estado al cambiar sin volver a suscribirse.
  `present = state == "baby" && baby ∈ {ausente, in, parents}`; `home = baby == "home"`.
  `offline` o `free` = no presente.
- Historial (máx. 32): `{id, segundos}`. Si se llena, sale la de menos segundos (nunca la actual).
- Días: `days_mask` de 7 bits (bit 0 = `mask_day`, el último día con gesto) y `gesture_day`.
- Todo menos la tabla va en un blob NVS `acc/state` (≈ 700 B). Se escribe al cambiar de fase, al
  completar el gesto y cada 10 min mientras cuenta.

## D3. Fases

| Fase | Qué muestra | Sale a |
|---|---|---|
| `SEARCHING` | incubadora vacía, "Ahora no hay bebés. Te avisaré" | `WAITING` en cuanto hay una IncuNest con bebé |
| `WAITING` | el bebé, halo que llama, "Un bebé te espera" | `WITH` al completar el gesto; `ENDING`/`FAREWELL` si se va |
| `WITH` | el bebé (estado normal); tras las 19:00 sin gesto hoy, "¿Le das la mano hoy?" y halo que llama | `ENDING` si deja de haber bebé; `FAREWELL` si se va a casa |
| `ENDING` | lo que diga la IncuNest; sin bebé y en línea: "Ha terminado su estancia" | vuelve a `WAITING`/`WITH` si el bebé vuelve; `FAREWELL` si llega "home"; `SEARCHING` (y elige) tras 2 h |
| `FAREWELL` | vista de casa congelada (no se aplican más estados de esa IncuNest), "¡Se ha ido a casa! Gracias por acompañarle" | `SEARCHING` (y elige) al tocar la pantalla o al cambiar el día |

Elegir: máximo de segundos del historial; desempate por `hash(client_id) ^ hash(id)`. `WAITING`
también es pegajoso: no se cambia a la favorita aunque tenga bebé después.

`engaged` = ya hubo gesto con el bebé actual: al volver de `ENDING` se va a `WITH` si `engaged`,
si no a `WAITING`.

## D4. Contar

Cada segundo, si fase `WITH`, `gesture_day == hoy` y el bebé está a la vista (`show_baby` o
`parents_view`), suma 1 s a la IncuNest actual. Sin hora válida, `hoy = -1` (cuenta igual tras un
gesto en esa sesión; no hay recordatorio).

## D5. Hora local y silencio

`TZ = CET-1CEST,M3.5.0,M10.5.0/3` (España) al iniciar el módulo. Recordatorio: hora ≥ 19,
`gesture_day != hoy`, fase `WAITING`/`WITH`, una vez por día. Silencio: hora ≥ 22 o < 9 →
`sound_set_quiet(true)`, que descarta `SOUND_BABY` y `SOUND_PARENTS` (no el clic ni el latido al
tocar). Solo en modo Acompañar.

## D6. Pegamento

- `commands.c`: un estado con el modo activo va a `accompany_ingest()`; este lo pasa a
  `twin_model_ingest()` solo si es de la IncuNest actual y no estamos en `FAREWELL`.
- Cambio de IncuNest: `accompany` publica `TWIN_EVT_PAIRING_CHANGED` con el id nuevo
  (twin_model se reinicia, `status` se publica) y luego aplica el payload guardado (suena
  `SOUND_BABY` por la transición normal, salvo silencio). `mqtt_link` ignora ese evento en este
  modo y se queda con la suscripción comodín.
- UI → `accompany`: `TWIN_EVT_ACC_INPUT` (`GESTURE` al completar "Coge mi mano", `TOUCH` al tocar
  la pantalla en `FAREWELL`).
- `accompany` → UI: campos `acc_*` del snapshot vía `twin_model_set_acc()`.
- El rechazo de la suscripción comodín solo se registra (sin rol el panel se queda buscando).
