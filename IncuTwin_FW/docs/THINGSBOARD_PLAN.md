# Plan de configuración de ThingsBoard para IncuTwin

Arquitectura objetivo (sin Firebase): el panel usa su única conexión MQTT a
`mon.medicalopenworld.org` para todo — provisión, estado del gemelo (shared
attributes), telemetría de uso y OTA.

```
IncuNest ──MQTT──> ThingsBoard ──rule chain──> shared attributes del IncuTwin ──push MQTT──> panel
                        ▲                                                          │
                   CRM/backend (REST) ── claiming/emparejamiento ── app móvil ◄────┘ (QR)
```

## 1. Modelado de entidades

**Device profiles**: `incunest` (el actual) e `incutwin` (nuevo).

**Nombre del dispositivo** = número de serie (`ITW-2607-0042`). El *label*
se rellena al emparejar con el nombre del donante.

**Relación incubadora↔gemelo**: relación de TB desde la IncuNest hacia sus
gemelos, tipo `MirroredBy` (una incubadora puede tener varios donantes).
La crea el backend al asignar el apadrinamiento desde el CRM.

**Donantes como Customers** de TB: cada donante es un Customer y su panel
se le asigna al emparejar. Esto da de serie visibilidad por donante,
dashboards por customer si algún día hacéis portal, y encaja con el
*claiming* nativo (ver §4).

## 2. Device profile `incutwin`

- Transporte: MQTT por defecto.
- **Device provisioning**: *Allow to create new devices* con la
  key/secret (las de prueba ya están en `config.h`; generar otras para el
  perfil de producción y guardarlas fuera de git).
- Regla raíz: rule chain propia "IncuTwin" (§5).
- **Perfil de alarmas** (opcional): inactividad del panel > 48 h → alarma
  "panel apagado" para detectar donantes que lo desenchufan.

## 3. Provisionamiento (ya implementado en firmware)

1. Panel sin token → MQTT usuario `provision` → `/provision/request` con
   `deviceName = SN`.
2. TB crea el dispositivo con perfil `incutwin` y devuelve el access
   token → NVS.
3. Primer connect: el panel publica *client attributes* una vez:
   `hwrev`, `pair_code`, `current_fw_version` (pequeño añadido pendiente
   en `tb_client.cpp`: hoy solo publica fw como telemetría).

Nota: si un SN ya existe (re-provisión tras borrar NVS), TB responde
`FAILURE`. Política recomendada: en el perfil, *Check for pre-provisioned
devices* desactivado, y para reactivar un panel usado → borrar el device
en TB o re-emitir credenciales desde soporte.

## 4. Emparejamiento con la app (claiming nativo de TB)

Usar [Device Claiming](https://thingsboard.io/docs/user-guide/claiming-devices/)
en lugar de lógica propia:

1. En el onboarding, el panel publica en `v1/devices/me/claim`:
   `{"secretKey": "<pair_code>", "durationMs": 86400000}` (ventana 24 h).
2. El QR lleva `sn` + `pair_code` a la app.
3. El backend de la app (usuario REST de TB) ejecuta
   `POST /api/customer/device/{SN}/claim` con `{"secretKey": ...}` en
   nombre del Customer del donante → TB valida el código y **asigna el
   panel al donante**. Sin backend propio de emparejamiento.
4. Tras el claim, una rule chain (evento *Entity assigned*) pone el
   label del device y notifica al CRM (REST call) para registrar el
   consentimiento RGPD con fecha.
5. El backend crea la relación `IncuNest --MirroredBy--> IncuTwin` según
   el apadrinamiento del CRM y fija el shared attribute `skin` a partir
   del país de la incubadora (tabla país→tono 0-5 en el backend).
6. Re-emparejamiento: factory reset del panel → nuevo `pair_code` → al
   reclamarlo otro Customer, TB lo reasigna (el claim des-asigna al
   anterior). Datos del donante anterior: borrarlos en CRM (RGPD).

## 5. Rule chains

**RC "IncuNest" (existente), rama nueva "estado del gemelo":**

1. *Filter script* — deduplicación: solo pasa si cambia el estado
   (thermo/photo/online) o si `hr` varía más de ±5 bpm o han pasado 60 s
   (compara con atributos del originador). Esto es lo que hace viable
   10k paneles en tu CPX42: sin esto, cada latido de cada IncuNest se
   multiplica por todos sus gemelos.
2. *Transformation script* — mapea telemetría IncuNest → esquema del
   gemelo:
   ```javascript
   var out = {
     online: true,
     thermo: msg.heater_on ? (msg.temp_stable ? "stable" : "heating") : "off",
     photo: msg.phototherapy === true,
     hr: Math.round((msg.heart_rate || 0) / 5) * 5,
     awake: msg.movement === true   // si existe el dato
   };
   ```
   (ajustar claves reales cuando pueda ver IncuNest_FW).
3. *Change originator* → *Related entities*, relación `MirroredBy`.
4. *Save attributes* (scope **SHARED_SCOPE**) → TB lo empuja por MQTT a
   cada panel suscrito a `v1/devices/me/attributes`.

**Inactividad**: nodo de *device activity* (o alarma de inactividad de la
IncuNest) → shared attr `online=false` en sus gemelos. El panel además
tiene su propio timeout local (`DATA_STALE_S`).

**RC "IncuTwin" (nueva, raíz del perfil):**

- Telemetría de uso → *Save timeseries* con **TTL 90 días**.
- `fw_state` → timeseries (para el dashboard de OTA).
- Evento *Entity assigned/unassigned* → REST al CRM.

## 6. OTA

1. *OTA updates* → subir `firmware.bin` (título `incutwin`, versión
   nueva, checksum MD5 autocalculado).
2. Despliegue escalonado: asignar primero el paquete a 2-3 dispositivos
   de prueba (asignación directa), verificar `fw_state=UPDATED`, y
   después asignarlo al device profile → toda la flota.
3. Dashboard "Firmware" de TB para seguimiento; alarma si un panel
   reporta `FAILED`.
4. Con 10k paneles, subir de versión por lotes (el perfil entero a la
   vez genera 10k descargas de ~1,5 MB = 15 GB en minutos; mejor
   asignar por grupos o en horario valle).

## 7. Dashboards mínimos

- **Flota**: total/online/offline, versión de firmware, mapa de países,
  suma de `hand_hours` (bonito para memoria anual de la ONG).
- **Soporte**: buscar por SN → estado, RSSI, uptime, último claim,
  donante asignado.

## 8. Seguridad y RGPD

- Rotar key/secret de provisión para producción; las de prueba quedan
  solo en el perfil de test.
- TLS: fase 2 — puerto 8883 con Let's Encrypt en el host (el subdominio
  `mon` debe seguir en "DNS only" de Cloudflare) y `WiFiClientSecure` en
  el firmware.
- Usuario REST dedicado para el backend de la app con contraseña fuerte
  (TB CE no tiene permisos granulares; limitar por red si es posible).
- Rate limits de transporte en el perfil (p. ej. 10 msg/s por device).
- Datos personales (nombre/email del donante): en TB solo el label y el
  Customer; el detalle vive en el CRM. Al desvincular: borrar Customer y
  datos en CRM.

## 9. Fases

| Fase | Qué | Criterio de salida |
| ---- | --- | ------------------ |
| 0 | Perfil `incutwin` + provisión en entorno de prueba, flashear 1 panel | Device creado solo, telemetría horaria visible |
| 1 | Firmware: estado por shared attributes (quitar Firebase) + rama de rule chain con 1 IncuNest real | El bebé reacciona a la incubadora real |
| 2 | Claiming + backend app + relación CRM + `skin` por país | Emparejamiento completo desde el QR |
| 3 | OTA end-to-end + dashboards + TTL + TLS 8883 | Actualización remota verificada |
| 4 | Prueba de carga (simulador de 10k MQTT, p. ej. `thingsboard-performance-tests`) en el CPX42 | Latencia y RAM estables antes de fabricar |

## 10. Cambios de firmware asociados (pendientes de OK)

1. Parsear el estado del gemelo desde shared attributes en
   `tb_client.cpp` (el parser y `g_state` ya existen) y eliminar el
   cliente Firebase.
2. Publicar client attributes al conectar (`hwrev`, `pair_code`).
3. Publicar el claim (`v1/devices/me/claim`) durante el onboarding.
