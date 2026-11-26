# incupray — IncuPray: cada domingo, todas las IncuTwin a la vez junto a los bebés

## Why

Cada IncuTwin está sola en una casa. Pablo (25-nov-2026) quiere un momento fijo cada semana en el
que todas se unan a la vez para estar con los bebés que hay en ese momento en las IncuNest: una
comunidad que se ve (cuántas IncuTwin y quiénes) y una razón más para volver. Usa los corazones de
`hmi-corazones` y va después de él. El nombre se queda en «IncuPray»; los textos describen el
momento sin hablar de rezar.

## What Changes

- **Cuándo**: cada domingo a las 20:00 hora de España, 10 minutos (fuera del horario nocturno).
  El horario lo publica la API del VPS (retenido); sin horario publicado no hay IncuPray.
- **Avisos**: la víspera y una hora antes, en la franja de arriba; al empezar, otra franja que
  lleva a la IncuPray. Si te la pierdes, un resumen al día siguiente («El domingo 214 IncuTwin
  estuvisteis juntas»), sin reproches.
- **Entrar**: se puede entrar en cualquier momento de los 10 minutos. Cuesta 3 corazones si los
  tienes; con menos, entras gratis. Nadie se queda fuera.
- **Qué se ve**: el mapa con todos los bebés que hay en ese momento en las IncuNest, cada uno con
  su cabeza de niño o niña en su país; el contador «214 IncuTwin juntas», lo que queda y los
  nombres de las IncuTwin que se han unido pasando abajo (el tuyo resaltado).
- **Estar con un bebé**: tocas el que quieras y lo ves en grande; manteniendo el dedo pulsado
  ganas 1 corazón por cada minuto (hasta 10). Puedes volver al mapa y elegir otro.
- **Al terminar**: «Gracias. 214 IncuTwin habéis enviado 1.380 corazones» y lo que has ganado.
  Los corazones de la IncuPray se suman al saldo, fuera del tope diario.
- **Nombre de la IncuTwin** en Ajustes (no lo pide el asistente): si no se pone, se ve el número
  de serie; hasta 16 caracteres; la API lo filtra antes de enseñárselo a las demás.
- **Demo**: una IncuPray simulada (nombres y bebés inventados) para las ferias.
- **API del VPS** (repo `incutwin-app`, `apps/api`): horario, lista de bebés en IncuNest, quién se
  une, nombres filtrados y suma de corazones, por el broker MQTT.

## Capabilities

### New Capabilities

- `incupray`: horario, avisos, entrada con apuesta, mapa de bebés, corazón por minuto, nombres,
  resumen y demo.

### Modified Capabilities

- `settings`: nombre de la IncuTwin.
- `device-status`: `status` lleva el nombre.
- `broker-link`: suscripción a los topics de la IncuPray y publicación de la participación.

## Impact

- Firmware: componente nuevo `components/incupray` (lógica pura de ventana, apuesta y corazones
  por minuto, con tests; pegamento MQTT), `ui` (pantalla IncuPray, franjas, nombre en Ajustes con
  el teclado de `ui_name`), `rewards` (sumar los corazones de la IncuPray), `cloud_status`
  (`name`), `mqtt_link` (topics nuevos), `demo_mode`.
- Broker: topics `incupray/next`, `incupray/live`, `incupray/summary` (los publica la API) e
  `incutwin/<id>/incupray` (los publica cada IncuTwin); ACL para los paneles y para la API.
- API del VPS (otro repo): servicio de la IncuPray y filtro de nombres.
- Legal: la política de privacidad debe decir que el nombre de la IncuTwin se enseña a otras
  IncuTwin durante la IncuPray.
