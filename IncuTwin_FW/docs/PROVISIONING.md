# IncuTwin — Fabricación, onboarding y vinculación

## 1. Serialización en fabricación

Identidad en dos capas:

- **MAC eFuse** del ESP32-S3: única e imborrable, es el respaldo. Si un
  panel no tiene NVS de fábrica, su SN es `ITW-XXXXXX` (últimos 3 bytes
  de la MAC).
- **SN legible** `ITW-YYWW-NNNN` (año+semana+secuencial), escrito en la
  partición NVS (namespace `factory`) por `tools/factory_provision.py`.

Flujo por panel (un comando):

```bash
pio run -t upload --upload-port COM5      # firmware
python tools/factory_provision.py --port COM5 --hwrev 1.2
```

El script lee la MAC, genera el SN, flashea la NVS de fábrica, crea la
etiqueta `tools/factory/label_<SN>.png` (QR + SN, 50×25 mm, 300 dpi) y
apunta el dispositivo en `tools/factory/devices.csv`.

Dependencias del PC de fabricación:
`pip install esptool esp-idf-nvs-partition-gen qrcode pillow`

## 2. Onboarding (primer arranque)

Si NVS `prov/done` no existe, arranca el asistente. Diseñado para no
teclear nada en el panel:

1. **Idioma** — ES/EN, español por defecto.
2. **Conecta tu móvil** — QR `WIFI:` para unirse al SoftAP
   `IncuTwin-XXXX` (pass `incutwin`). Al conectar, el portal cautivo se
   abre solo.
3. **Portal cautivo** (en el móvil) — red WiFi de casa (lista escaneada),
   contraseña, nombre, email y casilla de consentimiento **RGPD**
   (obligatoria).
4. El panel se conecta al WiFi (si falla, vuelve al paso 2 con aviso) y
   se **provisiona en ThingsBoard** automáticamente.
5. **Vincula la app** — QR con
   `https://app.medicalopenworld.org/pair?sn=<SN>&code=<código>` y botón
   *Terminar* → reinicio al modo normal.

Reset: mantener pulsado el engranaje de la pantalla principal →
confirmación → borra `prov` y vuelve al onboarding (el SN de fábrica no
se toca).

## 3. Provisión en ThingsBoard

Usa [Device Provisioning](https://thingsboard.io/docs/user-guide/device-provisioning/)
(*Allow to create new devices*):

- El panel conecta por MQTT con usuario `provision` y publica en
  `/provision/request`:
  ```json
  {"deviceName":"ITW-2607-0042",
   "provisionDeviceKey":"<key>","provisionDeviceSecret":"<secret>",
   "credentialsType":"ACCESS_TOKEN"}
  ```
- ThingsBoard crea el dispositivo (nombre = SN) y devuelve el access
  token, que se guarda en NVS. Las claves del perfil se configuran en
  `include/config.h` (`TB_PROVISION_KEY/SECRET` — ahora las del entorno
  de prueba; falta `TB_HOST`).

### Telemetría de uso (cada 5 min)

| Clave              | Descripción                                    |
| ------------------ | ---------------------------------------------- |
| `on_hours`         | horas totales encendido (acumulado de por vida)|
| `connected_hours`  | horas con la incubadora conectada              |
| `hand_hours`       | horas de "Agarra mi mano" (tocar al bebé)      |
| `hand_count`       | nº de interacciones "Agarra mi mano"           |
| `incubator_online` | estado actual                                  |
| `rssi`, `uptime_s` | diagnóstico                                    |
| `current_fw_title/version`, `fw_state` | estado OTA                 |

### OTA desde ThingsBoard

*OTA updates* → subir el `firmware.bin` (título `incutwin`, versión
nueva) → asignar al device profile o al dispositivo. El panel detecta el
atributo compartido `fw_version`, descarga por chunks MQTT (4 KB),
verifica MD5, reporta `fw_state` (DOWNLOADING → DOWNLOADED → VERIFIED →
UPDATING) y se reinicia. El binario está en
`.pio/build/crowpanel_advance_28/firmware.bin`.

## 4. Spec de vinculación (equipo de la app)

El QR del onboarding abre:

```
https://app.medicalopenworld.org/pair?sn=<SN>&code=<6 dígitos>
```

Flujo propuesto (backend con Firebase Admin):

1. La app (usuario autenticado) llama al backend con `sn` + `code`.
2. El backend verifica `code` contra `/incutwin/{sn}/pairing/code`
   (el puente lo replica desde el atributo `pair_code` del dispositivo en
   ThingsBoard, o se registra en fabricación).
3. Si coincide y `/owner` no existe → escribe
   `/incutwin/{sn}/owner = {uid, name, email}` y asigna la IncuNest
   apadrinada al donante (relación en ThingsBoard/CRM).
4. El backend fija `/incutwin/{sn}/state/skin` según el país de la
   IncuNest asignada (mapa país → tono 0-5).
5. Códigos de un solo uso: tras vincular, borrar `pairing/code`.

Nota: el panel guarda el código en NVS y lo muestra solo durante el
onboarding. Para re-vincular: factory reset (genera código nuevo).

## 5. Seguridad / RGPD

- Claves de provisión de TB: son del **entorno de prueba**; rota a las de
  producción antes de fabricar y no publiques `config.h` en repos
  públicos (o pásalas a un `secrets.h` fuera de git).
- MQTT va en claro (puerto 1883) en el entorno de prueba; en producción
  usa 8883 con TLS (`WiFiClientSecure` en `tb_client.cpp`).
- El consentimiento RGPD queda en NVS (`prov/gdpr`) y debe registrarse
  también en el backend al vincular (fecha + versión de la política).
