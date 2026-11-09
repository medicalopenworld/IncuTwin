# hmi-hold-my-hand — Tareas

Verificación en la IncuTwin de COM6, en modo demo (BOOT 2 s) y con `INCUTWIN_BUZZER_MUTE=y`
mientras Pablo no pueda tener sonido.

## 1. Silencio y configuración

- [ ] 1.1 Kconfig: `INCUTWIN_BUZZER_MUTE` y los tiempos `INCUTWIN_HAND_*` (D5)
- [ ] 1.2 `sound.c`: `tone()` siempre apagado con `INCUTWIN_BUZZER_MUTE` (D8)

## 2. Lógica del gesto (TDD en `test_apps`)

- [ ] 2.1 Tests Unity de `hand_hold`: escenarios de la spec `hold-my-hand` (estados, gracia,
      listo sin tocar, seguir tras completar, tocar directo, demo/sin sesión, espera 0 y 1800)
- [ ] 2.2 Componente `hand_hold` que los pasa
- [ ] 2.3 Test y función pura `hand_payload_build` en `cloud_status`

## 3. Red

- [ ] 3.1 `TWIN_EVT_HAND_HOLD` en `app_events`; `cloud_status` publica `incutwin/<id>/hand`

## 4. Pantalla

- [ ] 4.1 Textos ES/EN nuevos y "Coge mi mano"
- [ ] 4.2 `ui_home`: estados del gesto, anillo, diana, corazones, fondo, ojos, barra de estado
- [ ] 4.3 Etiqueta con el nombre

## 5. Verificación

- [ ] 5.1 `idf.py build` (producto) y build de pruebas silenciosa; `test_apps` en verde en COM6
- [ ] 5.2 Panel en COM6, modo demo: recorrer los escenarios de `twin-display` y `hold-my-hand`
- [ ] 5.3 Con broker: `mosquitto_sub -t 'incutwin/+/hand'` ve un aviso por gesto (cuando haya red)
- [ ] 5.4 Nota en `docs/BROKER-MQTT-CONTEXT.md` §9: mapear `incutwin/+/hand` en el gateway

## Ronda 2 (prueba en el panel)

- [x] Textos: "Mantén pulsado para estar mano a mano" (barra) y "El bebé está recibiendo calorcito" (fila); fila a dos líneas y barra que baja a 12 px si no cabe
- [x] Vueltas extra del anillo tras completar (`hh_held_ms`, tests en test_hand_hold.c)
- [x] Imagen `Corazon_IncuNest` a pantalla completa en el agradecimiento (gen_extra.py → `img_hand_thanks`)
- [x] Familia de piel oscura (`Baby_with_parents_black` → `img_baby_parents_black`) para `skin` 3..5
- [x] Clave `country` (ISO alfa-2) en el estado, etiqueta del país y Camerún en los escenarios de demo
- [x] Tonos de piel pulsables en ajustes durante la demo (`twin_model_demo_set_skin`)
- [ ] Verificación manual en el panel

## Ronda 3

- [x] Latido solo al tocar el icono del corazón (10 s o hasta otro toque); fuera la ventana tras "Bebé detectado" y el latido del gesto
- [x] Quitar el tono de piel más oscuro (5 tonos; `skin: 5` del estado se pinta como 4)
- [x] Páginas deslizables en demo (ui_pages.c): país/hospital/hora local y logros; bandera de Camerún en gen_extra.py
- [ ] Verificación manual en el panel
