# sim-server

Build de desarrollo (`CONFIG_INCUTWIN_SIM=y`) para probar la UI desde un navegador sin broker ni
incubadora. Nunca se distribuye.

## ADDED Requirements

### Requirement: Build aislada [test-unity]
Con `CONFIG_INCUTWIN_SIM=y` el firmware NO SHALL incluir ni arrancar el cliente MQTT ni la OTA, y
SHALL levantar un servidor HTTP en el puerto 80 de la IP de estación al conectar al WiFi,
imprimiendo `SIM: http://<ip>/` en el log. El modelo arranca como "sin vincular" con el enlace
simulado como correcto.

#### Scenario: Arranque sim
- **WHEN** arranca la build sim con WiFi
- **THEN** el log muestra la URL, no hay intentos de conexión MQTT y la pantalla dice "Sin
  IncuNest vinculada"

### Requirement: Página de control [manual]
`GET /` SHALL servir una página única (sin dependencias externas) con: botones de los 10
escenarios, controles campo a campo (termo, fototerapia, despierto, bebé dentro, en línea,
vinculada, lpm 0–220, tono 0–5) con botón "Aplicar", y una demo automática (intervalo 2–120 s,
Start/Stop).

#### Scenario: Uso desde el móvil
- **WHEN** se abre la URL desde un móvil en la misma WiFi y se pulsa "Alarma"
- **THEN** el panel muestra la alarma en < 500 ms

### Requirement: API de estado [test-unity]
`GET /state` SHALL devolver el modelo actual en JSON. `POST /state` SHALL aceptar `{"scenario":
"<id>"}` y/o campos sueltos (`online`, `linked`, `thermo`, `photo`, `hr`, `skin`, `awake`,
`baby`, `home`, `name`, `weight_g`, `age_d`), aplicar primero el escenario y luego los campos, y
responder con el estado resultante; cuerpo > 768 bytes, JSON inválido o escenario desconocido →
400.

#### Scenario: Escenario y refinado
- **WHEN** se envía `{"scenario":"sleep","hr":95,"name":"Lucía"}`
- **THEN** la respuesta muestra `baby_state: in`, `hr: 95`, `name: "Lucía"` y la barra dice
  "Lucía"

### Requirement: Inyección del payload real [test-unity]
`POST /incubator` SHALL aceptar exactamente el JSON de `incubators/<id>/state` (ver
`twin-state-ingest`) y pasarlo por el mismo parser que en producción, respondiendo 200 si se
aplicó y 400 con el motivo si se descartó. El `incubator_id` emparejado en sim es `SIM`.

#### Scenario: Payload del contrato
- **WHEN** se envía `{"incubator_id":"SIM","state":"baby","treatments":["heat","pulseox"],"bpm":138,"event_seq":1,"last_event":"baby_in"}`
- **THEN** aparece el bebé, suena "Bebé detectado" y un segundo envío idéntico no suena

### Requirement: Demo automática [manual]
`POST /demo` `{"run":true,"interval_s":10}` SHALL rotar los escenarios en bucle desde el
firmware (sobrevive al cierre del navegador) hasta `{"run":false}`.

#### Scenario: Feria
- **WHEN** se arranca la demo con 10 s y se cierra la pestaña
- **THEN** el panel sigue cambiando de escenario cada 10 s
