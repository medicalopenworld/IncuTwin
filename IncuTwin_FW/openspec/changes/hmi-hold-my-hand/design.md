# hmi-hold-my-hand — Diseño

## Contexto

La interacción actual vive en `ui_home.c` (`baby_touched`): PRESSED/RELEASED despiertan al bebé,
encienden el latido y cuentan uso. No hay estado ni tiempo. El panel solo habla con el broker
(nunca con ThingsBoard); ThingsBoard ya sabe convertir `{"hand_hold":1}` del panel en
`hand_hold_until` de la IncuNest.

## Decisiones

**D1 · Lógica del gesto en un componente puro `hand_hold`.** Máquina de estados en C sin LVGL ni
red: entradas `button()`, `touch(bool)`, `tick(now_ms)`, `set_available(bool, can_send)`; salidas
por consulta (`state`, `progress_permille`, `grace_left_s`) y un flanco `take_send()` que vale
`true` una vez por aviso a publicar. El tiempo entra como parámetro, así los tests de `test_apps`
la llevan con relojes sintéticos (60 s en microsegundos de test).
*Descartado*: meterla en `ui_home.c` con `lv_timer` y `lv_tick_get()` — no testeable sin pantalla;
meterla en `twin_model` — el gesto no es estado de la incubadora y no debe cruzar el event loop.

**D2 · El reloj lo da la UI.** Un `lv_timer` de 100 ms en `ui_home` llama a `hand_hold_tick` y
repinta anillo y textos solo si cambian. Todo en la tarea de LVGL (la de siempre: prioridad y
núcleo de `esp_lvgl_port`), sin tareas nuevas. 100 ms da un anillo fluido (600 pasos en 60 s) y
coste despreciable.

**D3 · Publicar por evento, no desde la UI.** Cuando `take_send()` da `true`, la UI postea
`TWIN_EVT_HAND_HOLD` (sin datos: el id emparejado y la sesión los conoce la red). `cloud_status`,
que ya publica `status` desde el event loop, construye el payload con una función pura
(`hand_payload_build`, testeada) y llama a `mqtt_link_publish(..., qos 1, retain false)`.
*Descartado*: `ui` dependiendo de `mqtt_link` — rompe la separación red → modelo → UI.

**D4 · Quién decide si se puede enviar.** La UI pasa `can_send = !demo && broker_connected &&
paired` en cada `apply`; la espera entre avisos (D5) la lleva `hand_hold`, que solo arma el flanco
si `can_send` y la espera ha vencido. Si no se puede, la última hora de envío no cambia (un gesto
en demo no consume la espera).

**D5 · Tiempos en Kconfig.** `INCUTWIN_HAND_MIN_S` (60), `INCUTWIN_HAND_GRACE_S` (10),
`INCUTWIN_HAND_READY_S` (20), `INCUTWIN_HAND_THANKS_S` (4) e `INCUTWIN_HAND_COOLDOWN_S` (0; el
valor previsto es 1800). Cambiarlos exige compilar; no hay comando remoto para ellos en esta
versión (*descartado* por ahora: `cmd/hand_cfg`, más superficie de ataque sin necesidad todavía).

**D6 · Ojos.** `ui_home_apply` deja de imponer `awake` mientras el gesto no esté en reposo; la UI
fija ojos abiertos en listo/gracia y cerrados en sosteniendo/completado, y al volver a reposo
restaura el `awake` real (ya guardado en `s_awake_real`).

**D7 · Presentación con objetos LVGL baratos.** Anillo = `lv_arc` sin knob ni clic; diana = dos
círculos con animación de opacidad; corazones = 4 `lv_image` de `img_heart` al 50 % con
animación de `y` y opacidad; la fila inferior = una etiqueta que sustituye a iconos y botón
fuera de reposo. La etiqueta del nombre es un `lv_label` con borde, oculto si `name == ""`.

**D8 · Silencio en compilación.** `INCUTWIN_BUZZER_MUTE` hace que `tone()` en `sound.c` apague
siempre el LEDC. Es el único punto que mueve el zumbador, así que cubre melodías, latido y clic.
*Descartado*: comando de consola de volumen — la melodía de arranque sonaría antes de poder
usarlo.

## Flujo

```
toque/botón ──► ui_home (LVGL) ──► hand_hold (puro)
                    ▲  lv_timer 100 ms ──► tick ──► anillo/textos/ojos
                    │                       └─► take_send() ──► app_events_post(TWIN_EVT_HAND_HOLD)
twin snapshot ──────┘ (show_baby, demo, broker, paired)            │
                                                                   ▼
                                    cloud_status (event loop) ──► mqtt_link_publish incutwin/<id>/hand
```

## Riesgos

- Un dedo quieto 60 s sobre el FT5x06 puede dar PRESS_LOST espurios: la gracia de 3 s los
  absorbe sin perder progreso.
- Sin el mapeo del gateway el aviso no llega a la IncuNest: se documenta en la propuesta y en
  `docs/BROKER-MQTT-CONTEXT.md` §9.
