# tb-stay-history — Historial de bebés acompañados y logros reales, calculados en ThingsBoard

## Why

La página de logros de la demo (días que has acompañado, racha, familias que acompañan, días del
bebé en la IncuNest) ha gustado, pero sus cifras son inventadas. Pablo quiere además un
**historial de los bebés que has acompañado**, para que el panel tenga continuidad cuando un
bebé se va a casa y llega otro. Decisión (10-nov-2026): **se calcula en el servidor**, no en el
panel. ThingsBoard ya sabe cuándo empieza y acaba cada estancia (`stay_id = SN:baby_seq`,
contrato v0.2 §3.4) y recibe cada "Coge mi mano" completado. El panel solo pinta lo que le
llega, así que el historial sobrevive a un restablecimiento de fábrica, a un cambio de panel y a
un nuevo emparejamiento.

## What Changes

- **Historial por panel en ThingsBoard** (capability nueva `stay-history`): cada panel IncuTwin
  guarda en un server attribute `itw_history` las estancias que ha acompañado (máx. 10), con
  país, fechas, cómo terminó (solo "a casa" o neutro), días de la estancia y sus propios
  "Coge mi mano". Lo mantiene una rule chain nueva `IncuTwin_history` con un script puro
  `tb/itw_history.js`, probado en local con Node como `itw_state_machine.js`.
- **Logros reales**: el mismo atributo lleva los días distintos en que el panel ha completado un
  "Coge mi mano" (todas las estancias), la racha de días seguidos y cuántas otras familias
  acompañan ahora a la misma IncuNest.
- **Entrega al panel**: mensaje retenido en `incutwin/<client_id>/cmd/history` (QoS 1, ≤ 1000
  bytes) cada vez que cambia. El panel ya está suscrito a `incutwin/<id>/cmd/#`; hace falta dar
  al cliente `thingsboard` del broker permiso de publicación en `incutwin/+/cmd/history`.
- **Disparadores**: eventos `baby_in`, `baby_back` y `baby_out` de la IncuNest (a todos sus
  paneles `MirroredBy`); "Coge mi mano" aceptado (al panel que lo hizo); y emparejamiento o
  desemparejamiento.
- **Emparejamiento coherente**: `tools/pair.py` y `tools/unpair.py` crean o borran también la
  relación `IncuNest —MirroredBy→ IncuTwin` en ThingsBoard y mantienen `itw_panel_count` en la
  IncuNest. Hoy solo tocan el broker, y sin la relación ThingsBoard no sabe a qué panel avisar.
- **País**: server attribute `country` (ISO 3166-1 alfa-2) en cada IncuNest, que lo pone MOW.
  Viaja en el historial y en `incubators/<id>/state` (el firmware ya lo acepta desde
  `hmi-hold-my-hand`).

Fuera de este cambio, en uno posterior de firmware (`hmi-stay-history`): leer
`cmd/history`, guardarlo en NVS, la página "Bebés que has acompañado" y que la página de logros
use estos datos fuera de la demo.

## Impact

- ThingsBoard (mon.medicalopenworld.org): rule chain nueva `IncuTwin_history`; ramas nuevas en
  la cadena de la IncuNest (eventos) y en `IncuTwin_hand_hold` (aceptado); nodo MQTT nuevo con
  topic `incutwin/${deviceName}/cmd/history`. Script de despliegue `tb/tb_apply_history.py`
  (idempotente, solo cadenas `IncuTwin*`).
- Broker: una ACL más en el rol `thingsboard` (`publishClientSend 'incutwin/+/cmd/history'`).
- `tools/pair.py`, `tools/unpair.py`: usan también la API de ThingsBoard (`tools/tb_api.py`).
- Contrato de eventos: v0.2 → v0.3 (nuevo §3.8 "Historial del panel"); `docs/BROKER-MQTT-CONTEXT.md`
  §3 añade el topic.
- Privacidad: el nombre solo entra en el historial con `itw_show_name = true` y solo se conserva
  tras el alta si el bebé se fue a casa (misma regla que el estado actual). Nunca peso, causas,
  desenlaces distintos de "a casa", ni medidas.
- Firmware del panel: ninguno en este cambio.
