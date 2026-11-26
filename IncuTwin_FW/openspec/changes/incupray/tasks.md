# incupray — Tareas

Requiere `hmi-corazones` terminado. Verificación en la IncuTwin de COM12 y con la API de pruebas.

## 1. Contrato

- [ ] 1.1 Topics y payloads de la IncuPray en `docs/BROKER-MQTT-CONTEXT.md` (y en
      `packages/contract` del repo `incutwin-app`)
- [ ] 1.2 ACL del broker: paneles (`incupray/#` lectura, `incutwin/<id>/incupray` escritura) y API

## 2. Lógica (TDD en `test_apps`)

- [ ] 2.1 Tests Unity de `incupray_logic`: ventana y lo que queda, avisos (víspera, 1 h, inicio),
      apuesta con y sin saldo, sin cobrar dos veces, segundos → corazones con tope 10, parar al
      cerrar, misma IncuPray al reconectar
- [ ] 2.2 `components/incupray/incupray_logic.c` que los pasa
- [ ] 2.3 Parseo de `next`, `live` y `summary` (tests con payloads de ejemplo, campos que faltan)
- [ ] 2.4 `status_payload_build` con `name` (test + código)

## 3. Pegamento

- [ ] 3.1 `mqtt_link`: suscripciones y publicación de la participación
- [ ] 3.2 `incupray.c`: NVS mínima, `TWIN_EVT_INCUPRAY`, `rewards_add_incupray`
- [ ] 3.3 `settings`: nombre de la IncuTwin en NVS; `cloud_status` publica `status` al cambiarlo

## 4. Pantalla

- [ ] 4.1 Textos ES/EN (sin hablar de rezar): avisos, contador, quedan, salir, gracias, resumen
- [ ] 4.2 Franjas de aviso e inicio; botón «IncuPray» en Inicio durante la ventana
- [ ] 4.3 Pantalla de la IncuPray: mapa con cabezas pulsables, contador, nombres pasando
- [ ] 4.4 Vista de un bebé con anillo de minutos y «+1» por minuto; «Salir»
- [ ] 4.5 Gracias al cerrar y resumen al día siguiente si no se entró
- [ ] 4.6 Ajustes: «Nombre de tu IncuTwin» con el teclado de `ui_name`
- [ ] 4.7 Demo: IncuPray simulada de 3 min

## 5. API (repo incutwin-app)

- [ ] 5.1 Tablas, publicación de `next`/`live`/`summary`, recogida de participaciones
- [ ] 5.2 Filtro de nombres y veto por panel
- [ ] 5.3 Probar con las IncuNest simuladas (SIM01..03) y la IncuTwin de COM12

## 6. Verificación

- [ ] 6.1 `idf.py -C test_apps` en COM12: todos los tests en verde
- [ ] 6.2 IncuPray de prueba programada a 5 min vista: avisos, entrar con apuesta, dedo 2 min →
      +2, nombre en la lista, gracias con los totales de la API
- [ ] 6.3 Política de privacidad actualizada antes de la primera IncuPray real
