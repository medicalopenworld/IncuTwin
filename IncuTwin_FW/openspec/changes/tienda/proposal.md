# tienda — Gorros, mantas y Navidad en la tienda de corazones (fase 1)

## Why

Pablo aprobó el 26-nov-2026 una tienda más rica que la de `hmi-corazones` (solo halo, fondo, guirnalda
e «Una IncuNest con tu nombre»): gorros y mantas para el bebé, fondos y efectos. Para eso cambia una
regla del 25-nov: lo que se compra **sí puede cambiar el dibujo del bebé**, con cuatro condiciones
(solo abrigo, la cara se ve entera, piel y sexo los marca la IncuNest, lo elegido lo lleva cada bebé
que acompañas). Propuesta visual: https://claude.ai/artifact/FBjREU4hUwgSrpYF1iyxSf.

La fase 1 es lo que tiene que estar en las IncuTwin antes del 24 de diciembre.

## What Changes

- **Gorros**: uno elegido para niño y otro para niña; la IncuTwin pone el que toca según el sexo del
  bebé (siempre lo hay: ThingsBoard lo sortea si la IncuNest no lo da). Básico de serie (el dibujo
  de hoy); colores Menta (15), Pollito (15) y Nube (20) para niño; Lila (15), Melocotón (15) y
  Menta (20) para niña.
- **Mantas**: una para todos los bebés. Básica de serie (azul o rosa según el bebé); Crema (15),
  Menta (15), Lavanda (20) y Melocotón (20).
- **Navidad**: gorro de Papá Noel (10, para los dos) y efecto Copos de nieve (10). Todo lo de
  Navidad, también la guirnalda, **se ve en la tienda siempre, pero solo se compra y se lleva del
  24 de diciembre al 6 de enero, ambos incluidos**. Antes se puede probar y dice «Desde el 24». La
  guirnalda se sigue ganando con 10 días de diciembre.
- **Todo cuesta** (Pablo, 26-nov): nada es gratis salvo lo de serie. Se descartó el regalo de
  bienvenida que se había propuesto.
- **Tienda nueva**: lo primero de Corazones es «Tienda», que abre una pantalla con pestañas Gorros,
  Mantas, Fondos y Efectos; tarjetas con muestra y precio; **Probar** enseña el bebé con lo elegido
  antes de gastar; se pregunta antes de gastar y sin saldo dice "Te faltan N corazones". «Una
  IncuNest con tu nombre» tiene su tarjeta en Corazones, debajo de la tienda.
- **Demo**: empieza con 50 corazones (antes 12) para que en las ferias se pueda comprar.
- **Revisión de la pantalla**: comandos de consola `pantalla` (captura), `tienda …` (navegar),
  `demo on|off` y `rewards comprar <id>` (solo en la demo), y `tools/screenshot.py`.
- **Guardado v2**: el blob crece (64 bits de desbloqueos y lo elegido nuevo) con migración desde la
  versión 1 sin perder saldo, racha, sellos, pasaporte ni desbloqueos.
- **Métricas**: `incutwin/<id>/metrics` añade gorros, manta, efecto y Papá Noel.

## Capabilities

### Modified Capabilities

- `rewards`: catálogo nuevo, temporada de Navidad, todo cuesta, demo con 50, guardado v2 con migración.
- `twin-display`: gorro, manta y Papá Noel en el bebé; Copos alrededor; pantalla Tienda y Probar.
- `device-metrics`: campos nuevos en `metrics`.

## Impact

- `components/rewards` (lógica + migración), `test_apps/main/test_rewards.c`, `test_metrics.c`.
- `tools/gen_assets.py`: entradas de paleta propias para gorro y manta, tablas de colores, sprite
  de Papá Noel; `components/assets` regenerado.
- `components/ui`: `baby_widget` (paleta de gorro/manta, Papá Noel), `ui_shop.c` nuevo (tienda y
  Probar), `ui_hearts.c` (entrada a la tienda), `ui_home.c` (copos, guirnalda desde el 24), textos.
- `cloud_status/metrics_payload.c`, consola `rewards`.
- Flash: menos de 100 KB más. Sin cambios en el broker ni en ThingsBoard.
