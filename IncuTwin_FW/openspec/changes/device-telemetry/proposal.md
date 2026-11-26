# device-telemetry — Registro de eventos de uso y foto ampliada

## Why

Pablo (26-nov-2026) quiere todos los datos de uso posibles de cada IncuTwin para afinar el juego,
analizar la campaña de socios y medir el uso. La foto horaria de `metrics` dice cómo está cada
IncuTwin, pero no cómo llegó ahí. Aún no hay ninguna desplegada: la telemetría completa tiene que
estar en el firmware antes de que salga la primera.

Diseño aprobado: `incutwin-app/docs/superpowers/specs/2026-11-26-telemetria-uso-design.md`.
Lo recibe el recolector del VPS (`incutwin-app/docs/superpowers/plans/2026-11-26-recolector-telemetria.md`).

## What Changes

- **Eventos** en `incutwin/<id>/ev`: lotes JSON con número de orden (`seq`), hora (`t`), segundos
  desde el arranque (`up`) y arranque (`b`). Catálogo: `boot`, `ota`, `net`, `onboard`, `gesture`,
  `heart`, `spend`, `seal`, `level`, `streak`, `passport`, `acc`, `incupray`, `redeem`, `screen`,
  `setting`, `demo`, `reset`.
- **Cola persistente** en la partición `storage` (8 MB, sin usar hasta ahora): unos 5.000 eventos,
  un evento sale de la cola solo tras el PUBACK; si se llena se descartan los más viejos y se cuentan.
- **`gen`**: sube con cada restablecimiento de fábrica (separa dueños). `seq` no se reinicia nunca.
- **Foto ampliada**: `metrics` lleva además `gen`, `fw`, `inc`, `phase`, `lang`, `vol`, `bright`,
  `demo_n`, `boot_n`, `q` y `lost`.
- La demo no genera eventos salvo `demo` al entrar y al salir.
- `metrics` deja de ir a ThingsBoard por el IoT Gateway: lo lee el recolector (rol `collector`).

## Capabilities

### New Capabilities

- `telemetry-events`: eventos, lotes, cola persistente, `gen` y `seq`.

### Modified Capabilities

- `device-metrics`: campos nuevos en la foto; destino el recolector en vez del IoT Gateway.
- `broker-link`: publicación con id de mensaje y aviso de PUBACK.
- `settings`: el restablecimiento de fábrica conserva el namespace `tel` y sube `gen`.

## Impact

- Firmware: componente nuevo `components/telemetry` (`tel_logic.c` puro con tests en `test_apps`,
  `tel_queue.c` sobre una interfaz de flash con doble de prueba, `telemetry.c` de pegamento);
  `mqtt_link` (PUBACK), `cloud_status` (foto), `rewards`, `accompany`, `ui` (pestañas, páginas,
  onboarding, reset), `settings`, `demo_mode`, `ota_update`, `storage`, `support_console` (`tel`).
- IncuPray (cambio `incupray`, aún sin implementar) debe llamar a `telemetry_log("incupray", …)`.
- Broker: rol `collector` (lo añade el plan del recolector, tarea 7).
- Depende de `hmi-corazones` commiteado en `feat/acompanar`.

## Operación (fábrica y reacondicionado)

- La partición `storage` **nunca** se borra en fábrica ni al reacondicionar una IncuTwin (nada de
  `esptool erase_flash` ni `erase_region` sobre ella; para empezar de cero basta
  `storage_factory_reset()` o borrar la `nvs`). En ella vive la cola con el `seq` más alto: si se
  borra, `seq` vuelve a empezar y el recolector descarta como duplicados (por cliente y `seq`) todos
  los eventos nuevos hasta pasar el último que ya tenía.
- Si se borra la `nvs` y `storage` no, el firmware sube `gen` al más alto de la cola y `boot_n` a uno
  más que el más alto: no vuelven atrás.
- `boot_n` y `demo_n` (NVS `tel`) cuentan toda la vida de la IncuTwin, a través de dueños: el
  restablecimiento de fábrica no los pone a 0. Para «arranques o demos de este dueño», filtrar por
  `gen` en el recolector.
