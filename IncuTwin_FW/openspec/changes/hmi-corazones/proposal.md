# hmi-corazones — Corazones, sellos, pasaporte y personalizar la IncuTwin

## Why

La racha de siete corazones crea el hábito de acompañar cada día, pero se queda en una semana:
no hay nada que se acumule ni que se consiga con el tiempo. Pablo (25-nov-2026) quiere gamificar
la IncuTwin con recompensas y puntos, con tres reglas: se premia lo que hace quien acompaña (nunca
lo que le pasa al bebé), sin culpa ni pérdidas, y sin competir entre personas ni pagar. Los
corazones serán además la moneda de la IncuPray (cambio `incupray`, que va después).

## What Changes

- **Corazones como moneda**: un saldo que se gana acompañando. +1 por el primer «Coge mi mano»
  del día, +2 por el súper corazón (una vez al día) y +1 cada 7 días seguidos de racha (los días
  sin bebés no la rompen). Como mucho 4 al día fuera de la IncuPray.
- **Niveles** por corazones ganados en total (gastar no baja de nivel): Manita (0), Caricia
  (10), Cosquillitas (30), Abrazo (75) y Achuchón (150).
- **Sellos**: primer «Coge mi mano», 7 días seguidos, 30 días seguidos, 30 días acompañando,
  primer súper corazón, 3 bebés acompañados y «Se fue a casa» (el bebé que acompañabas vuelve a
  casa; nunca en una salida neutra). No hay sellos de madrugada.
- **Pasaporte**: un sello con la bandera de cada país en el que has dado la mano a un bebé, con
  una estrella si alguno de sus bebés se fue a casa contigo.
- **Personalizar**: con corazones se desbloquean colores del halo en reposo y fondos de Inicio.
  Los colores que significan algo (calor, fototerapia, un bebé te llama) no cambian. En
  diciembre, una guirnalda de temporada se desbloquea gratis acompañando 10 días del mes.
- **Comodín de racha**: cada semana completa (7 días seguidos) guarda un «abrazo guardado»
  (máximo 2) que cubre un día sin acompañar sin romper la racha.
- **En pantalla**: una pestaña nueva **Corazones** en la barra (cinco botones: casa, sobre de
  Notificaciones, corazón, pollito del Historial y rueda) con el saldo y el nivel, la racha, los
  sellos, el pasaporte y la tienda; la página de la racha de Inicio enseña también el saldo y el
  nivel; el corazón que vuela a la racha suma «+1»; cada sello nuevo o subida de nivel sale en la
  franja de arriba (no se guarda en Notificaciones) y lleva a Corazones.
- **«Una IncuNest con tu nombre»** en la tienda, por 365 corazones: se canjea una vez, el panel lo
  avisa al broker y lo cumple MOW (Pablo, 26-nov-2026).
- **Métricas en ThingsBoard**: uso y recompensas en `incutwin/<id>/metrics` (al conectar, cada hora
  y tras cada cambio), que el IoT Gateway lleva a ThingsBoard.
- **Latido**: el icono del corazón late al ritmo real del bebé y su burbuja ya no dice el número.
- **Demo**: saldo, sellos y pasaporte simulados que empiezan de cero en cada demo; nunca tocan
  los de verdad.
- El borrado de fábrica borra corazones, sellos, pasaporte y desbloqueos (la IncuTwin puede
  cambiar de manos). Guardarlos en el servidor es de una fase posterior.

## Capabilities

### New Capabilities

- `rewards`: saldo de corazones, cómo se ganan y se gastan, niveles, sellos, pasaporte, comodín
  de racha, catálogo de personalización y el canje de «Una IncuNest con tu nombre».
- `device-metrics`: `incutwin/<id>/metrics` y `incutwin/<id>/redeem` hacia ThingsBoard.

### Modified Capabilities

- `twin-display`: pantalla Corazones y barra de cinco botones, saldo y nivel en la página de la
  racha, «+1» en el corazón que vuela, aviso de sello o nivel en la franja, halo y fondo
  personalizados, latido del icono del corazón sin número.
- `settings`: el restablecimiento de fábrica borra también las recompensas.

## Impact

- Firmware: componente nuevo `components/rewards` (`rewards_logic.c` puro, probado en
  `test_apps`, y `rewards.c` con NVS y eventos), `ui` (páginas, personalizar, franja), `storage`
  (namespace `rewards`, borrado en reset de fábrica), `accompany` (súper corazón y días sin bebés
  como entrada), `stay_log` (estancias para «bebés acompañados» y «se fue a casa»), `main`.
- Broker: el rol `gateway` lee `incutwin/+/metrics` e `incutwin/+/redeem` (`tools/broker_bootstrap.py`).
- IoT Gateway: dos entradas nuevas en `mapping` (docs/BROKER-MQTT-CONTEXT.md §9); en ThingsBoard,
  un aviso a MOW por cada canje. Sin cambios en la API.
