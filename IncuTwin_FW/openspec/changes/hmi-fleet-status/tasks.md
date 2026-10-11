# hmi-fleet-status — Tareas

Rama `feat/fleet-status`. Un commit por stage del loop (skill `loop-engineering`). Verificación en
el panel de COM12 contra el broker real antes de pasar a review.

## 1. Red: tests que fallan (`test_apps`)

- [ ] 1.1 `test_backoff_status.c`: `status_payload_build` con `status_info_t` → los tres escenarios
  de `device-status` (emparejado con hora, pánico sin hora, peor caso ≤ 320 B)
- [ ] 1.2 `test_diag.c` (nuevo): `diag_payload_build` → informe completo exacto, sin red ni
  emparejado, peor caso ≤ 1024 B; `diag_reset_str` tabla completa + `other`
- [ ] 1.3 `test_commands.c`: `cmd_parse_diag` → vacío, `true`, `false`, ausente, no booleano, JSON
  inválido
- [ ] 1.4 `test_apps/main/CMakeLists.txt`: compilar `diag_payload.c` y la cabecera nueva; build de
  `test_apps` en rojo (fallan por enlazado o por aserción, no por sintaxis)

## 2. Green: implementación mínima

- [ ] 2.1 `components/diagnostics`: `include/diag_payload.h` (`status_info_t`, `diag_info_t`,
  `diag_reset_str`, `diag_payload_build`) y `diag_payload.c` en C puro
- [ ] 2.2 `components/cloud_status/status_payload.[ch]`: firma nueva con `status_info_t`, tope
  320 B, claves en el orden de la spec
- [ ] 2.3 `components/commands/commands_parse.[ch]`: `cmd_parse_diag(payload, len, &erase)`
- [ ] 2.4 `components/mqtt_link`: `mqtt_link_reconnect_count()`
- [ ] 2.5 `components/twin_model`: `twin_model_last_seq()`
- [ ] 2.6 `components/diagnostics/diagnostics.[ch]`: `diagnostics_init()` (motivo de reinicio y
  presencia de core dump cacheados), `diagnostics_collect_status()`, `diagnostics_collect()`,
  `diagnostics_request(erase)` (timer + `diag_task` + publicación + borrado)
- [ ] 2.7 `components/commands/commands.c`: despacho de `diag` (unicast 0 ms, flota 0–60 s, flag
  ignorado por flota con WARN)
- [ ] 2.8 `components/cloud_status/cloud_status.c`: rellenar `status_info_t` con el recolector
- [ ] 2.9 `components/support_console/console_cmds.c`: comando `diag`
- [ ] 2.10 `main/app_main.c`: `diagnostics_init()` tras `usage_init()`; `main/CMakeLists.txt`
- [ ] 2.11 `test_apps` en verde en COM12 (`idf.py -C test_apps -p COM12 flash` +
  `tools/serial_capture.py`)

## 3. Herramientas

- [ ] 3.1 `tools/mqtt_cmd.py --diag <SN> [--erase-coredump] [--timeout 10]`: publica `cmd/diag` y
  espera el informe en `incutwin/<id>/diag`; si el SUBACK falla, dice que falta la ACL
  `subscribePattern 'incutwin/+/diag'` en el rol `publisher`

## 4. Verificación en el panel (COM12, broker real)

- [ ] 4.1 `status` al conectar con los campos nuevos (`tools/mqtt_cmd.py --status`)
- [ ] 4.2 `diag` por unicast < 2 s; dos seguidos → uno solo y WARN
- [ ] 4.3 `diag` por consola sin WiFi
- [ ] 4.4 Pánico forzado → `reset: panic`, `coredump: true`; `--erase-coredump` → `false` sin
  reiniciar
- [ ] 4.5 `diag` por flota → retardo en el log entre 0 y 60 s, flag de borrado ignorado
- [ ] 4.6 Medir el coste de `diagnostics_collect()` (log con µs) y anotar en el commit de verify

## 5. Docs y cierre

- [ ] 5.1 `docs/BROKER-MQTT-CONTEXT.md` §3: payload de `status` nuevo, topic `diag`, ACL del rol
  `publisher`, aviso sobre `all/cmd/diag`
- [ ] 5.2 README (sección de comandos y herramientas) y `components/diagnostics/README.md` breve
- [ ] 5.3 `openspec archive hmi-fleet-status`
