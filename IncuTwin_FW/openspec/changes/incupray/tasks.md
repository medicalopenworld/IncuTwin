# incupray — Tareas

Requiere `hmi-corazones` terminado. Verificación en la IncuTwin de COM12 y con la API de pruebas.

**26-nov-2026, dark launch:** el firmware de las IncuTwin que se entregan hoy lo lleva todo (1–4) y
está apagado hasta que la API publique `incupray/next`. Lanzarla = solo servidor (5), sin OTA.

## 1. Contrato

- [x] 1.1 Topics y payloads de la IncuPray en `docs/BROKER-MQTT-CONTEXT.md` (falta copiarlos a
      `packages/contract` del repo `incutwin-app`, con la API)
- [x] 1.2 ACL del broker: paneles (`incupray/#` lectura, `incutwin/<id>/incupray` escritura) y API
      (de momento el rol `publisher`; aplicadas el 26-nov con `broker_bootstrap.py`)

## 2. Lógica (TDD en `test_apps`)

- [x] 2.1 Tests Unity de `incupray_logic`: ventana y lo que queda, avisos (víspera, 1 h, inicio),
      apuesta con y sin saldo, sin cobrar dos veces, segundos → corazones con tope 10, parar al
      cerrar, misma IncuPray al reconectar (no cobrar dos veces y reconectar: en `incupray.c`, NVS
      por id; probado en COM12)
- [x] 2.2 `components/incupray/incupray_logic.c` que los pasa
- [x] 2.3 Parseo de `next`, `live` y `summary` (tests con payloads de ejemplo, campos que faltan)
- [x] 2.4 `status_payload_build` con `name` (test + código)

## 3. Pegamento

- [x] 3.1 `mqtt_link`: suscripciones y publicación de la participación
- [x] 3.2 `incupray.c`: NVS mínima, `TWIN_EVT_INCUPRAY`, `rewards_add` / `rewards_spend`
- [x] 3.3 `settings`: nombre de la IncuTwin en NVS; `cloud_status` publica `status` al cambiarlo

## 4. Pantalla

- [x] 4.1 Textos ES/EN (sin hablar de rezar): avisos, contador, quedan, salir, gracias, resumen
- [x] 4.2 Franjas de aviso e inicio; botón «IncuPray» en Inicio durante la ventana
- [x] 4.3 Pantalla de la IncuPray: mapa con cabezas pulsables, contador, nombres pasando
- [x] 4.4 Vista de un bebé con anillo de minutos y «+1» por minuto; «Salir»
- [x] 4.5 Gracias al cerrar y resumen al día siguiente si no se entró
- [x] 4.6 Ajustes: «Nombre de tu IncuTwin» con el teclado de `ui_name`
- [x] 4.7 Demo: IncuPray simulada de 3 min (botón BOOT: pulsación corta tras el último escenario;
      consola `incupray demo`)
- [x] 4.8 Revisión visual de Pablo: pantalla, vista del bebé, gracias, fila del nombre y teclado
      (OK con la demo, 26-nov; sigue apagada hasta que se decida lanzarla)

## 5. API (repo incutwin-app)

- [ ] 5.1 Tablas, publicación de `next`/`live`/`summary`, recogida de participaciones
- [ ] 5.2 Filtro de nombres y veto por panel
- [ ] 5.3 Probar con las IncuNest simuladas (SIM01..03) y la IncuTwin de COM12

## 6. Verificación

- [x] 6.1 `idf.py -C test_apps` en COM12: todos los tests en verde (158, 26-nov)
- [ ] 6.2 IncuPray de prueba programada a 5 min vista (26-nov, sin API: `next`/`live`/`summary` a mano,
      aviso de inicio, entrar gratis sin saldo, `hold 61` → +1, participación al entrar, cada minuto y al
      salir recibida en el broker; falta con la API y con el dedo de verdad): avisos, entrar con apuesta, dedo 2 min →
      +2, nombre en la lista, gracias con los totales de la API
- [ ] 6.3 Política de privacidad actualizada antes de la primera IncuPray real
