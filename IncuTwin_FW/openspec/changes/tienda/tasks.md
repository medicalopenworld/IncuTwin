# tienda — Tareas (fase 1)

Verificación: tests de la lógica en el PC con gcc (atajo) y en `test_apps` on-target en COM12;
después el firmware en COM12.

## 1. Lógica (TDD)

- [x] 1.1 Tests de `rewards_logic`: gorros por sexo, manta, Navidad 24-dic..6-ene (comprar, poner,
      ver), todo cuesta, migración v1 → v2, métricas nuevas
- [x] 1.2 `rewards_logic.c` / `.h` que los pasa (v2, `unlocked` de 64 bits, `rw_unlock` con fecha y motivo)
- [x] 1.3 `rewards.c`: `state2` con migración desde `state`; `metrics_payload`; consola `rewards`

## 2. Dibujo

- [x] 2.1 `gen_assets.py`: entradas de paleta propias de gorro y manta, tablas de colores, sprite de
      Papá Noel; regenerar `components/assets` y las previsualizaciones
- [x] 2.2 `baby_widget`: `baby_set_outfit()` (gorro, manta, Papá Noel) componiendo con el tono de piel
- [x] 2.3 Copos en Inicio; guirnalda solo del 24-dic al 6-ene

## 3. Tienda

- [x] 3.1 Textos ES/EN de la tienda
- [x] 3.2 `ui_shop.c`: pestañas, Niño/Niña, tarjetas, Probar con su propio bebé, comprar con diálogo
- [x] 3.3 Corazones: «Tienda» en lugar de la lista de filas

## 4. Verificación

- [x] 4.1 `idf.py build` sin avisos nuevos (26-nov: 3,17 MB, +41 KB; 0,98 MB libres en el slot).
      `test_apps` compila. 28 tests de `rewards` y `metrics` pasan en el PC con gcc
- [x] 4.2 `test_apps` en COM12 (26-nov: 168 tests, 0 fallos)
- [x] 4.3 Firmware en COM12: migración con los corazones de verdad, comprar y probar en la demo,
      Navidad con la fecha forzada por consola. Hecho por consola en la demo (compras, Papá Noel y
      copos en Inicio). 26-nov: migración OK en ITW-0A1284 (saldo 2, racha 3,
      «Primera mano», TG; tras reiniciar lee `state2` sin migrar otra vez; regalo 1). Falta comprar y
      probar en la pantalla (demo) y Navidad a mano
- [x] 4.4 Primera revisión de Pablo (26-nov): miniaturas mal, textos montados, cosas gratis, tienda
      no la primera. Arreglado: todo cuesta (sin regalo), tienda lo primero en Corazones, cuatro
      pestañas, tarjetas de 72 × 88 en dos filas, miniaturas reducidas a mano, «Elegido» sin gris,
      demo con 50 corazones, IncuNest con tu nombre en Corazones
- [x] 4.5 Capturas de todos los estados con `tools/screenshot.py` (normal y demo, Navidad forzada)
- [ ] 4.6 Segunda revisión visual de Pablo
