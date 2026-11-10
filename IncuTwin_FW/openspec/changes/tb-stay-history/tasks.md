## 1. Lógica pura (sin tocar el servidor)

- [x] 1.1 `tb/test_history.js` con los escenarios de `specs/stay-history` (rojo)
- [x] 1.2 `tb/itw_history.js`: `stay_open`, `stay_close`, `hold`, `sync`, recorte a 10, stats,
      privacidad, tamaño del mensaje ≤ 1000 bytes (verde)
- [x] 1.3 `country` en `itw_state_machine.js` (estado del broker) y caso en `test_sm.js`

## 2. Despliegue en ThingsBoard (cada paso con OK de Pablo: es el servidor real)

- [ ] 2.1 `tb/tb_apply_history.py` (idempotente, solo cadenas `IncuTwin*`): cadena
      `IncuTwin_history` (atributos del panel + IncuNest → script → guardar `itw_history` → nodo
      MQTT `incutwin/${deviceName}/cmd/history` retenido)
- [ ] 2.2 Ramas: eventos `baby_in`/`baby_back`/`baby_out` de la IncuNest → panel(es) `MirroredBy`
      → `IncuTwin_history`; aceptado en `IncuTwin_hand_hold` → panel → `IncuTwin_history`;
      telemetría `itw_history_sync` del panel → `IncuTwin_history`
- [ ] 2.3 Comprobar la cola (`SEQUENTIAL_BY_ORIGINATOR`) tras el cambio de originador (design,
      riesgo de concurrencia)
- [ ] 2.4 ACL en el broker: `addRoleACL thingsboard publishClientSend 'incutwin/+/cmd/history' allow`
      (infra o `tools/broker_bootstrap.py`)

## 3. Emparejamiento

- [ ] 3.1 `tools/pair.py` / `tools/unpair.py`: relación `MirroredBy`, `itw_panel_count`,
      `itw_history_sync` vía `tools/tb_api.py`; sin credenciales de TB, aviso y solo broker
- [ ] 3.2 Emparejar `incutwin-DEEF3C` con la 353 también en ThingsBoard

## 4. Documentación

- [ ] 4.1 Contrato de eventos v0.3: §3.8 "Historial del panel"
- [ ] 4.2 `docs/BROKER-MQTT-CONTEXT.md` §3 y §4: topic `cmd/history` y ACL

## 5. Verificación

- [ ] 5.1 `node tb/test_history.js` y `node tb/test_sm.js` en verde
- [ ] 5.2 Con la IncuNest de pruebas: un "Coge mi mano" desde el panel en COM3 sube `holds`, y
      `mosquitto_sub -t 'incutwin/incutwin-DEEF3C/cmd/history' -v` muestra el historial retenido
- [ ] 5.3 Alta de prueba con `baby_outcome = 1` → la entrada pasa a `"home"`
