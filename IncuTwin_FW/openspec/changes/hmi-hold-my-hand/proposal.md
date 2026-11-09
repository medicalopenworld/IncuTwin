# hmi-hold-my-hand — "Coge mi mano" con gesto de un minuto y aviso a la IncuNest

## Why

Hoy tocar al bebé solo lo despierta y hace sonar el latido mientras dura el toque; no llega nada
a la familia del hospital. "Coge mi mano" es la funcionalidad que da sentido al panel para el
público principal (niños y mujeres de 25 a 55 años): un gesto sostenido, con significado, que
acaba en un acompañamiento real para la familia de la IncuNest. ThingsBoard ya tiene el camino de
vuelta (`tb/itw_hand_hold.js`, contrato v0.2 §3.5: telemetría `{"hand_hold":1}` del panel →
`hand_hold_until` en la IncuNest); falta que el panel lo emita y que el gesto se sienta como tal.

Decisiones de producto de Pablo (9-nov-2026, mockups en `docs/mockups/coge-mi-mano-v2.png`):
gesto de mínimo 60 s que se puede alargar sin límite; si se suelta, 10 s para volver sin perder
el progreso; al pulsar el botón el bebé abre los ojos y al tocarle los cierra; al completar,
"Gracias por tu calorcito"; espera entre envíos de 30 min implementada pero a 0 por ahora; el
nombre puesto en la IncuNest se ve en el panel y, si no hay, no se muestra nada.

## What Changes

- **Gesto "Coge mi mano"** (capability nueva `hold-my-hand`): máquina de estados reposo → listo
  (botón pulsado) → sosteniendo → gracia (dedo levantado, ≤ 10 s) → completado. Progreso
  visible como un anillo coral alrededor del halo que se llena en 60 s; corazones que suben;
  texto por fases en la fila inferior y en la barra de estado. Tocar al bebé directamente desde
  reposo también empieza el gesto.
- **Ojos invertidos**: listo y gracia → ojos abiertos ("te espera"); sosteniendo y completado →
  ojos cerrados ("se calma en tu mano"). Al terminar vuelve al estado real.
- **Aviso a la IncuNest**: al llegar a 60 s el panel publica una vez `incutwin/<client_id>/hand`
  (QoS 1, no retenido) con `{"hand_hold":1,"incubator_id":"<id>","hold_s":60}`. No en demo ni sin
  sesión MQTT. Espera mínima entre envíos configurable (Kconfig, 0 por defecto; 1800 previsto).
- **Etiqueta con el nombre** del bebé sobre la manta cuando el estado trae `name`; oculta si no.
- **Texto del botón**: "Agarra mi mano" pasa a "Coge mi mano" / "Hold my hand".
- **Buzzer silenciable en compilación** (`CONFIG_INCUTWIN_BUZZER_MUTE`, por defecto `n`), para
  pruebas en sitios donde no puede sonar. No cambia el producto.

## Capabilities

### New Capabilities

- `hold-my-hand`: tiempos, estados, envío y espera entre envíos del gesto.

### Modified Capabilities

- `twin-display`: el requisito "Agarra mi mano" se sustituye por la presentación del gesto; nueva
  etiqueta con el nombre; texto del botón.
- `broker-link`: la disciplina de tráfico admite el mensaje `hand`.
- `sound`: el latido al tocar suena solo mientras se sostiene; buzzer silenciable en compilación.

## Impact

- **Firmware**: nuevo componente `hand_hold` (lógica pura, testeable), `ui` (pantalla principal,
  textos), `cloud_status` (payload y publicación de `hand`), `app_events` (evento nuevo),
  `sound` (silencio en compilación), `app_config` (Kconfig), `test_apps`.
- **Broker**: el rol `incutwin` ya publica en `incutwin/%c/#`; sin cambio de ACL.
- **ThingsBoard / gateway** (fuera de este repo de firmware): el conector MQTT del gateway debe
  mapear `incutwin/+/hand` a telemetría del panel para que la rule chain `IncuTwin` lo lleve a
  `IncuTwin_hand_hold`. Hasta entonces el mensaje llega al broker y nadie lo consume.
- **Fuera de alcance**: recuento por países en la IncuNest, quitar alarmas, páginas deslizables.
