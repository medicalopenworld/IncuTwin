# incupray — Diseño

## Context

- Va después de `hmi-corazones`: necesita el saldo de corazones, el teclado del nombre del bebé
  (`ui_name`), las cabezas de niño y niña, el mapa (`world_map`, `twin_geo`) y la franja.
- El panel solo habla con el broker Mosquitto (nunca con ThingsBoard ni con la API directamente).
- Los paneles en modo Acompañar ya reciben `incubators/+/state`; los emparejados fijos no.
- La API del VPS (repo `incutwin-app`, `apps/api`, Node + Fastify + mqtt.js) ya escucha el
  broker y tiene Postgres: es quien puede juntar participaciones, filtrar nombres y contar.
- Decisiones de Pablo (25-nov-2026): unirse no excluye a nadie; apuesta de 3 corazones que se
  recupera quedándose; 1 corazón por minuto de dedo; mapa de bebés en vez de lucecitas; nombre
  opcional en Ajustes (si no, el número de serie); API del VPS; IncuPray al final.

## Goals / Non-Goals

**Goals:**
- Momento semanal sincronizado en todas las IncuTwin, con el mapa de bebés, nombres y corazones.
- Contrato MQTT claro entre panel y API, con lógica de ventana y corazones probada en `test_apps`.

**Non-Goals:**
- La IncuPray en la app móvil (la 1.0 sale el 20-dic con alcance cerrado).
- Varias IncuPray por semana o en otras horas por país (el horario es único; la API lo cambia).
- Chat, mensajes entre IncuTwin o cualquier ranking.

## Decisions

### 1. El horario lo manda la API, retenido
`incupray/next` retenido `{"id":"2026-12-06","start":1765047600,"duration_s":600}`. El panel no
calcula domingos: la API puede mover, saltar o hacer una especial (Nochebuena) sin firmware nuevo.
- *Alternativa descartada*: calcularlo en el panel. Cada cambio sería una OTA.

### 2. La lista de bebés la da la API, no el estado de cada IncuNest
Durante la ventana, la API publica cada 10 s `incupray/live`
`{"id","count","hearts","names":[...≤40],"babies":[{"inc","country","sex","skin"}]}`.
- *Alternativa descartada*: que cada panel se suscriba a `incubators/+/state`. Los emparejados
  fijos no tienen ese permiso y cientos de paneles leyendo todo multiplica el tráfico.
- Los bebés del mismo país se reparten en un círculo de 10 px alrededor del punto de `twin_geo`.

### 3. La participación la cuenta la API
Cada panel publica `incutwin/<id>/incupray` (al unirse, cada 60 s, al salir). La API guarda por
`id` de IncuPray y panel el último `held_s`/`hearts` y publica `count` y `hearts` en `live`;
al cerrar, `incupray/summary` retenido `{"id","count","hearts"}`.
- Los corazones del panel los decide el panel (lógica pura, mismo cálculo); la API solo suma
  lo que le llega. Sin servidor de corazones en esta fase: no hay antitrampas.

### 4. El nombre viaja en `status` y lo filtra la API
`status` retenido lleva `name` si hay; la API lo recorta a 16, quita lo que no sea
letra/número/espacio de las fuentes del panel y lo cambia por el número de serie si está en la
lista de palabras prohibidas. Solo los nombres filtrados salen en `live.names`.
- *Alternativa descartada*: filtrar en el panel. La lista cambia y no debe requerir OTA.

### 5. Componente `incupray`
`incupray_logic.c` (puro: ventana abierta/quedan, avisos que tocan, apuesta, segundos → corazones
con tope, misma IncuPray al reconectar) e `incupray.c` (MQTT, NVS mínima: id de la última
IncuPray en la que se cobró la entrada y su `held_s`, para no cobrar ni perder tras un reinicio).
Suma corazones con una función de `rewards` (`rewards_add_incupray`), fuera del tope diario.

### 6. Pantalla
Pantalla propia (no una página de Inicio) que se abre desde la franja o desde Inicio mientras la
ventana está abierta (un botón «IncuPray» sustituye a «Coge mi mano» esos 10 min). Mapa a toda
pantalla con las cabezas pulsables; vista de un bebé con el anillo de minutos de «Coge mi mano»;
barra de navegación oculta y «Salir» arriba. Los nombres pasan de uno en uno (no caben cientos).

### 7. Tareas y eventos
Sin tareas nuevas: el pegamento corre en la tarea del bucle de eventos y los mensajes MQTT entran
por el handler de `mqtt_link` como el resto (copian y publican un evento). Eventos nuevos:
`TWIN_EVT_INCUPRAY` (ventana, live, summary) hacia la UI; la UI publica el dedo como hoy el gesto.

```
API ─incupray/next,live,summary─▶ mqtt_link ─▶ incupray ─TWIN_EVT_INCUPRAY─▶ ui
ui (dedo) ─▶ incupray ─▶ rewards (+corazones) ; incupray ─incutwin/<id>/incupray─▶ API
```

### 8. API (repo incutwin-app)
Servicio `incupray` en `apps/api`: tabla `incupray_events(id, start, duration_s)` y
`incupray_participation(event_id, device_id, held_s, hearts, updated_at)`; publica `next`
retenido al arrancar y al cambiar, `live` cada 10 s en la ventana, `summary` retenido al cerrar.
Contrato en `packages/contract` (zod). ACL: la API publica `incupray/#` y lee `incutwin/+/incupray`
y `incutwin/+/status`; los paneles leen `incupray/#` y publican su `incutwin/<id>/incupray`.

## Risks / Trade-offs

- [Cientos de paneles publicando a la vez cada 60 s] → mensajes pequeños, QoS 1, sin retener; el
  broker ya soporta el estado de las IncuNest.
- [Nombres ofensivos que pasan el filtro] → la API puede vetar un panel (nombre → número de serie)
  sin OTA.
- [Un bebé en el mapa sale de la IncuNest durante la IncuPray] → desaparece en el siguiente
  `live`; si se estaba con él, se vuelve al mapa con un mensaje neutro ("Ya no está en la
  IncuNest"), sin alarma.
- [Sin hora o sin conexión] → no hay IncuPray; los segundos ganados se publican al volver.
- [Privacidad] → nombre opcional, filtrado, solo durante la IncuPray; falta la línea en la
  política de privacidad (bloquea la publicación).

## Migration Plan

Firmware y API se despliegan por separado: sin `incupray/next` los paneles no hacen nada, así que
el firmware puede ir antes. La primera IncuPray se programa cuando la API está desplegada.
Rollback: borrar el retenido `incupray/next`.

## Open Questions

- ¿Primera IncuPray en Nochebuena (especial) o el primer domingo después de desplegar?
- ¿Quién mantiene la lista de palabras prohibidas en la API?
