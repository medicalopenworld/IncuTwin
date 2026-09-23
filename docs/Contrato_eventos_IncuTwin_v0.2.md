# Contrato de eventos IncuTwin — borrador v0.2

**Fecha:** 14 sept. 2026 · **Estado:** borrador para revisión MOW + NEXT · **Sustituye a v0.1.**
**Fuentes:** `medicalopenworld/IncuNest` **rama `dev`** (último merge 11 sept 2026) y `medicalopenworld/IncuTwin` (FW 1.1.0, 3 ago 2026). ThingsBoard: tenant `Medical Open World` tal como quedó el 14 sept (perfiles `IncuNest`, `IncuNest_HMI`, `IncuTwin`, `IncuTwin_bridge`).

## Qué cambia respecto a v0.1

La v0.1 se escribió sobre `master` (v17.0.0, junio). En `dev` el firmware IncuNest ya tiene un **módulo de perfil de bebé** que resuelve casi todo lo que en v0.1 había que inferir con heurísticas:

| Tema | v0.1 (master) | v0.2 (dev) |
|---|---|---|
| Entrada de bebé | No existía; heurística por pulsioxímetro / modo SKIN / Auto Air | **Asistente obligatorio** en el HMI al encender cualquier terapia (calor, humedad, fototerapia): la enfermera identifica al bebé. IncuNest publica `baby_seq`, `baby_admission_epoch`… como client attributes. |
| Alta | No existía; timeout T_out | **`PROFILE_DISCHARGE` explícito** con `baby_outcome` (sobrevive / fallece / traslado) y `baby_stay_days`, publicado como evento con su propio timestamp. Además fin de sesión de cuidado implícito cuando toda terapia se apaga. |
| Canguro | No contemplado | **`baby_kangaroo_event`**: el bebé sale con la madre y vuelve. Encaja con la pantalla "con sus papás" del panel. |
| Timestamps | Ninguno; hora de servidor TB | Sincronización horaria (NTP/NITZ) y **cola de eventos con `ts` propio** (8 entradas, store-and-forward). |
| Alarmas | 9 booleanos genéricos (`temp_alarm`…) | 19 claves específicas (`skin_temp_high_alarm`, `air_temp_low_alarm`, `mains_alarm`, `hmi_link_alarm`, `sb_door_alarm`…). |
| Identidad TB | Device = CCID de la SIM | Device = **`IncuNest-<SN>`**. El HMI **deja de ser device propio** (los 72 devices `IncuNest_HMI` quedarán huérfanos al desplegar `dev`). |
| SensorBoard | No existía | Nueva placa: `sb_door_open`, `sb_lux`, 3 temperaturas y 3 humedades, `sb_link_ok`. |
| Decisión "sin intervención humana en el hospital" | Asumida | **Ya no es cierta ni necesaria**: la enfermera identifica al bebé para poder encender la incubadora. Es un paso que ya existe por motivos clínicos (NTE, exposición por bebé), no un paso añadido para IncuTwin. |

Consecuencia: la heurística de presencia de v0.1 desaparece del contrato. `baby_present` en firmware (tarea nº 1 de v0.1) **ya no es necesaria** para lanzar: `baby_seq` + `Control_active`/`Phototherapy_active` la sustituyen con más fiabilidad. Queda como mejora opcional para detectar sesiones sin asistente (imposibles en `dev`, salvo cancelación del wizard).

---

## 0. Resumen ejecutivo

1. **IncuNest `dev` sabe quién está dentro, cuándo entró, cuándo salió y cómo.** El contrato pasa de inferir a traducir. Riesgo residual: depende de que la enfermera use bien el asistente y el alta; el alta no explícita se cubre con "sesión de cuidado terminada".
2. **`baby_seq` es el identificador natural del episodio** (`stay_id`). Se genera en el firmware, es único por incubadora y viaja en todos los payloads del bebé.
3. **El desenlace existe como dato** (`baby_outcome`: 0 desconocido, 1 sobrevive, 2 fallece, 3 traslado). La decisión pendiente de v0.1 ya no es "si se puede saber" sino "qué se cuenta al padrino". Propuesta abajo (§3.4, `baby_out`).
4. **Datos que nunca salen de TB**: `baby_name`, `baby_weight_g`, `baby_gest_weeks`, `baby_discharge_cause`, temperaturas, SpO2, alarmas. El webhook lleva categorías, no medidas.
5. **Firmware IncuTwin**: sin cambios respecto a v0.1 (quitar Firebase, shared attributes, `hand_hold`, TLS). La pantalla "con sus papás" pasa a dispararse por evento canguro real, no por pérdida de señal.
6. **ThingsBoard**: la rama "estado del gemelo" se cuelga de la rule chain raíz de IncuNest con un filtro `incutwin_enabled == true` por incubadora. Los 72 devices `IncuNest_HMI` hay que planificar retirarlos con el despliegue de `dev`.

---

## 1. Inventario de telemetría IncuNest (`dev`) → ThingsBoard

### 1.1 Transporte e identidad

| Aspecto | Valor en `dev` | Comentario |
|---|---|---|
| Protocolo | MQTT (SDK ThingsBoard actualizado, cambio `mb-thingsboard-sdk-bump`), WiFi **o** GPRS | RPC: `restart`, `getDiag`, `setWifi`. |
| **Nombre del device** | **`IncuNest-<SN>`** (`_n` si colisión) | Devices antiguos siguen con nombre = CCID. Para el contrato, `incubator_id` = atributo `SN`, no el nombre. |
| Un solo device por incubadora | Motherboard únicamente (spec `single-cloud-device-identity`) | HMI republica sus diagnósticos bajo el motherboard (`hmi_heap_*`, `hmi_fw_version`). Perfil `IncuNest_HMI` → obsoleto. |
| Periodo WiFi | 5 s (`TX_WIFI_PUBLISH_MS`) | Payload completo. |
| Periodo GPRS | 60 s actuando · 180 s solo fototerapia · 3600 s standby | `transport_policy.h`. Grupos de claves por canal: en WiFi no van `CELLULAR`, `DIAG` ni `CALIBRATION`. |
| Hora | **Sincronizada** (SNTP en WiFi, NITZ/`AT+CNTP` en GPRS) | Los eventos de bebé llevan `{"ts": <ms>, "values": {...}}`; si el reloj no está sincronizado, van sin `ts` y TB los sella al llegar. |
| Cola offline | **Sí, para eventos de bebé**: 8 entradas FIFO en RAM (`CLOUD_QUEUE_CAP`), se pierde la más antigua si se llena | La telemetría periódica sigue sin bufferizar. **Cola en RAM: un reset pierde los eventos pendientes.** Ver §1.5. |

### 1.2 Claves publicadas

Solo se listan las que afectan al contrato o son nuevas en `dev`. El resto (energía, diagnóstico, calibración, celular) sigue como en v0.1.

**Perfil de bebé — client ATTRIBUTES** ("quién está dentro ahora"; se sobreescriben, sin historia). Se envían cuando cambia el ocupante (`attributesDirty`):

| Clave | Tipo | Notas |
|---|---|---|
| `baby_seq` | uint32 | **Identificador del episodio.** `0` = no hay ningún perfil en los 3 slots. ⚠️ El "ocupante" es el perfil bajo terapia activa **o, si no hay terapia, el más reciente creado**. Es decir, `baby_seq ≠ 0` no significa "bebé dentro"; hay que cruzarlo con `Control_active`/`Phototherapy_active` (§3.4). |
| `baby_name` | string | **Dato personal. No sale de TB.** |
| `baby_gest_weeks`, `baby_weight_g` | uint | Clínicos. No salen de TB. |
| `baby_admission_epoch` | uint32 s | `0` = reloj no sincronizado al crear el perfil. |
| `baby_kangaroo_count`, `baby_phototherapy_min`, `baby_thermo_min`, `baby_humidity_min` | uint | Acumulados por bebé. Útiles para el resumen final al padrino ("N días de calor, M sesiones de luz"). |
| (vacío) | — | Cuando no hay perfiles: todos a `0`/`""`. |

**Perfil de bebé — TELEMETRÍA de eventos** (con `ts` propio, encolados):

| Evento | Claves | Cuándo |
|---|---|---|
| Peso | `baby_seq`, `baby_weight_g` | Cada vez que la enfermera introduce un peso (curva de crecimiento). Clínico; no sale de TB. |
| **Canguro** | `baby_seq`, `baby_kangaroo_event: 1`, `baby_kangaroo_count` | La enfermera responde "canguro" en el diálogo de salida del bebé (se muestra cuando la incubadora pasa a idle). El perfil sigue activo. |
| **Alta** | `baby_seq`, `baby_admission_epoch`, `baby_discharge_epoch`, `baby_outcome` (0–3), `baby_discharge_cause` (0–6, solo si fallece), `baby_stay_days`, acumulados | `PROFILE_DISCHARGE` explícito desde el HMI. Archiva el perfil. Un perfil que nunca recibe alta y es expulsado por FIFO (3 slots) se archiva con `outcome=0` **sin publicar evento de alta**. |

**Control y terapias** (cada periodo; semántica igual que v0.1):

| Clave | Tipo | Notas |
|---|---|---|
| `Control_active` | bool | Calor o humedad activos. |
| `Control_mode` | `"AIR"`/`"SKIN"` | **Sigue siendo flanco** (solo primer mensaje tras activar). Latch en TB. |
| `Temp_desired`, `Hum_desired` | float | Solo con control activo. |
| `Phototherapy_active` | bool | Siempre presente. |
| `Air_temp`, `Skin_temp`, `Humidity` | float | `Skin_temp ≈ 0` sin sonda. |
| `SpO2`, `SpO2_SQI`, `PI`, `HR1..3`, `HR1..3_SQI` | float/int | Solo con señal (`SQI > 0`). `PPG_snapshot_v1` nuevo: forma de onda, no la usamos. |
| `fan_rpm`, `fan_pwm`, `fan_has_fb` | int | Nuevos. Irrelevantes para el contrato. |

**SensorBoard** (nueva, `TX_GROUP_SENSORBOARD` en ambos canales):

| Clave | Tipo | Uso en el contrato |
|---|---|---|
| `sb_door_open` | bool | Candidato a **`awake`/interacción** ("alguien está atendiendo al bebé") en v0.3. No se usa en v0.2. |
| `sb_lux` | float | Candidato a día/noche en el panel (v0.3). |
| `sb_temp0..2_C`, `sb_hum0..2_pct`, `sb_link_ok`, `sb_env_used` | — | No. |

**Alarmas** (booleanos, transiciones; latch en TB). Nuevo juego de 19 claves:

`hum_alarm`, `air_temp_high_alarm`, `air_temp_low_alarm`, `skin_temp_high_alarm`, `skin_temp_low_alarm`, `air_TC_alarm`, `skin_TC_alarm`, `air_sensor_alarm`, `skin_sensor_alarm` (modo SKIN), `skin_sensor_air_alarm` (modo AIR), `heater_alarm`, `heater_sensor_alarm`, `fan_alarm`, `air_blocked_alarm`, `power_alarm`, `mains_alarm`, `hmi_link_alarm`, `sb_link_alarm`, `sb_door_alarm`.

Desaparece `temp_alarm` genérica (v0.1). Para `heat = alarm` se usan las térmicas: `air_temp_*`, `skin_temp_*`, `*_TC_alarm`, `heater_*`, `air_sensor_alarm`, `skin_sensor_alarm`, `air_blocked_alarm`.

### 1.3 Cómo detecta lo que necesitamos (`dev`)

| Necesidad | Fuente | Fiabilidad |
|---|---|---|
| **Entrada** | Asistente obligatorio → attributes `baby_seq` nuevo + `Control_active ∨ Phototherapy_active` | Alta. Único hueco: la enfermera cancela el asistente (la terapia queda apagada, así que tampoco hay bebé tratándose). |
| **Calor / humedad / fototerapia** | `Control_active`, `Temp_desired` vs medida, alarmas térmicas; `Phototherapy_active` | Alta. |
| **Pulsioximetría** | Presencia de `SpO2_SQI`/`HRx_SQI` | Alta. |
| **Canguro** | `baby_kangaroo_event` | Depende de que la enfermera responda al diálogo. |
| **Alta** | `baby_outcome` + `baby_discharge_epoch` | Depende del alta explícita. Fallback: sesión idle prolongada (§3.4). |
| **Desenlace** | `baby_outcome` 0–3 | Existe. Decisión de producto pendiente sobre qué se comunica. |
| **Despierto/dormido** | Nada directo. `sb_door_open` es proxy de "atención". | v0.3. |

### 1.4 Cambios recomendados en firmware IncuNest (Pablo) — revisados

Menos y más pequeños que en v0.1:

1. **Publicar `baby_active_seq`** (uint32; `0` cuando no hay terapia) junto a los attributes del bebé, o un bool `baby_in_care`. Hoy `baby_seq` mezcla "bajo terapia" con "el último creado"; TB puede cruzarlo con `Control_active`, pero una clave explícita elimina una carrera entre el attribute update y la telemetría periódica. Dos líneas en `baby_cloud.cpp`.
2. **Publicar el evento de alta también cuando un perfil se archiva por FIFO** (con `outcome=0`). Si no, un bebé que nunca recibió alta explícita desaparece sin evento. Alternativa: TB lo deduce al ver un `baby_seq` nuevo bajo terapia (§3.4 lo cubre), pero el evento es más limpio.
3. **Persistir la cola de eventos en NVS/LittleFS**, no solo RAM. Un reset con GPRS caído pierde el alta o el canguro.
4. `Control_mode` como estado en cada payload (igual que v0.1, sigue pendiente).
5. Shared attribute `hand_hold_until` consumido por la incubadora (igual que v0.1).
6. Opcional: `baby_present` por sonda/PPG como *cross-check* del asistente. Ya no bloquea nada.

### 1.5 Impacto del despliegue de `dev` en ThingsBoard (fuera del contrato pero urgente)

- Los **72 devices `IncuNest_HMI`** dejan de conectarse. Sus dashboards (`hmi_*`) deben re-apuntar al motherboard (mismas claves, spec lo garantiza). Plan: marcar el perfil como *legacy*, no borrar hasta que la flota esté en `dev`.
- Devices con nombre = CCID y devices `IncuNest-<SN>` conviven. El label y el atributo `SN` son la referencia; nunca el nombre.
- La cola de bebé + `ts` propio significa que **pueden llegar eventos con fecha pasada**. La rule chain de eventos IncuTwin debe usar `metadata.ts`, no la hora de proceso, y tolerar desorden.

---

## 2. Estado del firmware IncuTwin (FW 1.1.0)

Sin cambios respecto a v0.1 §2 (reutilizar UI/sonido/identidad/uso/OTA; adaptar `tb_client` a shared attributes, `hand_hold` y TLS; tirar `firebase_stream` y el portal con datos personales; la app Expo y `servidor-push` no sirven de base). Dos ajustes:

- La pantalla **"con sus papás"** se dispara por el estado `parents` que llega por shared attribute (evento canguro), y ya no por cualquier flanco de bajada de `show_baby`. La pérdida de WiFi vuelve a mostrar la vista offline normal. Es un cambio de 10 líneas en `ui_main.cpp` (`ui_apply_state`).
- El esquema de shared attributes del panel gana un campo: `baby` pasa de bool a **enum string** `"none" | "in" | "parents" | "out"`. Compatibilidad: si llega bool, `true→"in"`, `false→"none"`.

---

## 3. Contrato de eventos v0.2

### 3.1 Principios

Iguales que v0.1: TB emite significado, no medidas; un episodio = un `stay_id`; idempotente y ordenable (`event_id`, `seq`); el bebé nunca sale por fallo de red; `schema` versionado. Se añade:

6. **`stay_id` = `"<incubator_id>:<baby_seq>"`.** Lo genera el firmware, no TB. Si TB se reinicia o pierde estado, el episodio sigue siendo reconocible.
7. **Los eventos con `ts` del dispositivo se respetan.** `ts` en el envelope es el del firmware cuando existe; `received_at` es el de TB. La app ordena por `seq`, muestra `ts`.

### 3.2 Identidades

| Entidad | Identificador | Origen |
|---|---|---|
| Incubadora | `incubator_id` = atributo `SN` | Device TB perfil `IncuNest` (nombre `IncuNest-<SN>` o CCID legado) |
| Episodio | `stay_id` = `SN:baby_seq` | Firmware IncuNest |
| Panel | `panel_id` = SN `ITW-…` | Device TB perfil `IncuTwin` |
| Padrino | `uid` Firebase Auth | Nunca en TB |
| Panel↔incubadora | Relación TB `IncuNest —MirroredBy→ IncuTwin` | Backend MOW |
| App↔incubadora | Firestore `sponsorships/{uid}` | Cloud Function tras pago |
| Incubadoras habilitadas para IncuTwin | Server attribute `incutwin_enabled: true` | MOW, por incubadora. Filtro de entrada de la rama en la rule chain. |
| Hospital | Customer TB | Monitorización clínica; independiente de IncuTwin |

### 3.3 Sobre común

```json
{
  "schema": "incutwin.events/0.2",
  "event_id": "8b6e0f5a-…",
  "event": "baby_out",
  "seq": 4127,
  "ts": "2026-09-14T10:32:07Z",
  "received_at": "2026-09-14T10:32:09Z",
  "incubator_id": "IN3-2601-0042",
  "stay_id": "IN3-2601-0042:17",
  "payload": { }
}
```

### 3.4 Estado derivado y eventos

**Estado por incubadora (server attributes en TB):**

```
online, last_seen, seq
care_active     bool   = Control_active ∨ Phototherapy_active
baby_seq        int    (attribute del firmware, latch)
stay_id         string | null
baby_state      "none" | "in" | "parents" | "out"
heat            "off" | "heating" | "stable" | "alarm"
photo, spo2     bool
hr              int | null
control_mode    latch de Control_mode
```

**`heat`**: igual que v0.1, con las alarmas térmicas nuevas: `alarm` si alguna de `air_temp_high/low`, `skin_temp_high/low`, `air_TC`, `skin_TC`, `heater`, `heater_sensor`, `air_sensor`, `skin_sensor`, `air_blocked` está latcheada; `off` si `¬Control_active`; `stable` si `|medida − Temp_desired| ≤ 0.5 °C` sostenido 5 min; `heating` en el resto.

**`baby_state`** (máquina de estados en TB; sustituye a la heurística de v0.1):

```
none ──[baby_seq = N ≠ 0 ∧ care_active]──────────────────▶ in      (baby_in, stay_id = SN:N)
in   ──[baby_kangaroo_event con seq N]─────────────────────▶ parents (baby_parents)
parents ─[care_active de nuevo con seq N]──────────────────▶ in      (baby_back)
parents ─[T_parents = 4 h sin volver]──────────────────────▶ out     (baby_out reason "not_returned")
in|parents ─[evento alta con seq N]────────────────────────▶ out     (baby_out reason "discharged")
in|parents ─[¬care_active durante T_idle = 6 h]────────────▶ out     (baby_out reason "session_ended")
in|parents ─[baby_seq = M ≠ N ∧ care_active]──────────────▶ out+in  (baby_out reason "replaced" y baby_in de M)
out  ──[baby_seq = N mismo ∧ care_active en < 24 h]───────▶ in      (baby_back: se reabre el mismo stay; caso "alta por error")
```

`T_parents` y `T_idle` son server attributes por incubadora. Los defaults (4 h / 6 h) son propuestas: un canguro de más de 4 h es raro; una incubadora apagada 6 h con el mismo perfil casi siempre es un alta no registrada.

| Evento | Disparo | `payload` |
|---|---|---|
| **`baby_in`** | `none→in`, o `out→in` con seq nuevo | `{ "heat", "photo", "spo2", "admitted_at": ts \| null }` — `admitted_at` = `baby_admission_epoch` si ≠ 0 |
| **`baby_parents`** *(nuevo)* | `in→parents` | `{ "kangaroo_count": n }` |
| **`baby_back`** *(nuevo)* | `parents→in`, o reapertura de stay | `{ "away_min": 42, "heat", "photo", "spo2" }` |
| **`treatment_changed`** | Cambio de `heat`/`photo`/`spo2` en `in` (no en `parents`) | `{ "heat", "photo", "spo2", "changed": [...] }` |
| **`heartbeat`** | Cada 60 s en `in`/`parents` con `online` | `{ "hr", "hr_quality", "heat", "photo", "spo2", "baby_state" }` |
| **`baby_out`** | Cualquier transición a `out` | `{ "reason": "discharged" \| "session_ended" \| "not_returned" \| "replaced", "duration_days": n, "summary": { "thermo_h": …, "photo_h": …, "kangaroo": n }, "outcome": ver abajo }` |
| **`incubator_offline` / `incubator_online`** | Igual que v0.1 | Igual que v0.1 |

**`outcome` en `baby_out` — propuesta para decidir.** El dato existe (`baby_outcome` 0–3). Tres opciones:

- **A. No enviar nada.** El padrino ve "el bebé ha dejado la incubadora" siempre. Simple, seguro, pero cuando el desenlace es bueno se pierde el momento más valioso del producto ("se ha ido a casa").
- **B. Enviar solo lo positivo** (recomendada): `"outcome": "home"` cuando `baby_outcome = 1`; `"outcome": null` en los demás casos (0, 2, 3) sin distinguirlos. Así la app puede celebrar el alta a casa, y ante un fallecimiento o traslado dice lo mismo que ante un desconocido: "ha dejado la incubadora". No se comunica una muerte por push y tampoco se miente.
- **C. Enviar la categoría** (`home`, `transferred`, `deceased`, `unknown`). Requiere decidir con el hospital y con asesoría legal/psicológica cómo se comunica. No para v0.2.

`baby_discharge_cause` **nunca** sale de TB con ninguna opción.

### 3.5 Canal inverso ("Coge mi mano")

Igual que v0.1 §3.5, con el mecanismo ya creado: la Function publica en el device `IncuTwin_cloud_bridge` (`POST /api/v1/<token>/telemetry`) `{"incubator_id": "<SN>", "hand_hold_minutes": 5}`; la rule chain `IncuTwin_bridge` valida que el SN existe y tiene `incutwin_enabled`, y escribe `hand_hold_until` como shared attribute de la IncuNest y de sus paneles. Solo se acepta en `baby_state ∈ {in, parents}`.

### 3.6 Entrega y 3.7 Offline

Iguales que v0.1 (webhook HMAC a una Cloud Function, `event_id` idempotente, `seq` monótono, reintentos + reconciliación cada 5 min, fan-out en Firestore, FCM por topic). Dos matices nuevos:

- **Eventos con `ts` pasado**: tras un corte GPRS, la cola del firmware descarga altas o canguros de hace horas. TB los procesa en orden de llegada pero con su `ts`; la máquina de estados usa `ts` para calcular `duration_days`/`away_min` y la app los muestra con su hora real.
- **Reset de la incubadora con cola llena**: se pierden eventos. Si TB ve `baby_seq` nuevo bajo terapia sin haber visto el alta del anterior, cierra el anterior con `reason: "replaced"`. Nadie se queda con un bebé "dentro" para siempre.

---

## 4. Reparto de trabajo — revisado

| # | Tarea | Quién | Estado |
|---|---|---|---|
| 1 | TB: perfiles `IncuTwin`/`IncuTwin_bridge`, rule chains base, device bridge, customers de prueba | Pablo (hecho con Claude) | **Hecho 14-09** |
| 2 | TB: rama "estado del gemelo" en la rule chain de IncuNest con filtro `incutwin_enabled`, máquina de estados §3.4, shared attributes a paneles, webhook HMAC | Pablo | Siguiente sesión. Ya no depende de `baby_present`. |
| 3 | FW IncuNest (`dev`): `baby_active_seq`, alta por FIFO, cola persistente, `Control_mode` estado, `hand_hold_until` | Pablo | Ninguno bloquea la v0.2; el 1º y 2º son los que más simplifican la rule chain |
| 4 | FW IncuTwin: quitar Firebase, shared attributes → `g_state`, `baby` como enum, `hand_hold`, TLS, `secrets.h` | Pablo | `secrets.h` en parche listo |
| 5 | Firebase MOW: Auth, Firestore, Functions `tbEvents`/`holdHand`/reconciliación, FCM | NEXT | Puede empezar con los fixtures de §3.3–3.4 |
| 6 | App: onboarding, pairing QR, estado sin números, alta a casa (opción B), notificaciones | NEXT | Depende de 5 |
| 7 | Plan de retirada de los 72 devices `IncuNest_HMI` al desplegar `dev` | Pablo | Antes del despliegue |
| 8 | Decisión producto: `outcome` (A/B/C), `T_parents`, `T_idle`, límite "Coge mi mano" | MOW | Bloquea 6 solo en la pantalla de alta |

Hito de integración: **una alta con `baby_outcome = 1` en `TEST-incubadora` llega a un teléfono como "se ha ido a casa"**.

---

## 5. Decisiones abiertas

1. **`outcome`**: A, B o C (§3.4). Recomendación: **B**.
2. **`T_parents` (4 h) y `T_idle` (6 h)**: validar con una enfermera de un hospital piloto.
3. **Qué se muestra en el HMI** al recibir `hand_hold_until`. Nada sonoro.
4. **`sb_door_open` y `sb_lux`** como fuente de `awake`/día-noche en v0.3: barato y sin micrófono. ¿Se descarta el micrófono?
5. Micrófono: si §5.4 basta, se cierra.
6. Titularidad del proyecto Firebase `incutwinapp`; purga del historial de IncuTwin (parche listo, `push --force` pendiente); tenant `Demo`.
7. Apple/Google: donaciones vs IAP (igual que v0.1).
