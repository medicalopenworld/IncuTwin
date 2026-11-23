# hmi-accompany — Tareas

Verificación en la IncuTwin de COM12: `test_apps` on-target y después la app.

## 1. Lógica (TDD en `test_apps`)

- [x] 1.1 Tests Unity de `acc_logic`: escenarios de la spec `accompany`
- [x] 1.2 `components/accompany/acc_logic.c` que los pasa
- [x] 1.3 `cmd_parse_pair` reconoce `{"mode":"accompany"}` (test + código)
- [x] 1.4 `twin_status_of`: estados del modo (test + código)
- [x] 1.5 `status_payload_build` con `acc` (test + código)

## 2. Pegamento

- [x] 2.1 `storage`: blob y borrado de `acc` en el reset de fábrica
- [x] 2.2 `accompany.c`: tabla en PSRAM, NVS, tick 1 s, TZ, silencio, eventos
- [x] 2.3 `pairing.c`, `commands.c`, `mqtt_link.c` (comodín), `twin_model` (`acc_*`), `cloud_status`
- [x] 2.4 `sound_set_quiet`
- [x] 2.5 UI: textos ES/EN, halo que llama, toque en la despedida, gesto → `accompany`,
      corazones en la página del país
- [x] 2.6 `app_main`

## 3. Herramientas y broker

- [x] 3.1 `tools/pair.py <panel> accompany` (rol `incubators-all`) y `tools/unpair.py` lo quita

## 4. Verificación

- [x] 4.1 `idf.py -C test_apps` en COM12: todos los tests en verde
- [ ] 4.2 `idf.py build` y flash en COM12; poner el panel en modo Acompañar y comprobar que elige
      una IncuNest con bebé
