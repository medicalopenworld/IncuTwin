# IncuTwin — Esquema de datos y puente ThingsBoard → Firebase

## Arquitectura

```
IncuNest ──MQTT──> ThingsBoard ──rule chain/servicio──> Firebase RTDB ──SSE──> IncuTwin (CrowPanel 2.8")
```

El panel **no** habla con ThingsBoard directamente: se suscribe por streaming
(SSE) a un nodo de Firebase Realtime Database que un puente mantiene
actualizado con la telemetría y atributos de la IncuNest.

## Nodo que consume el panel

Ruta (configurable en `include/config.h`):

```
/incutwin/{DEVICE_ID}/state
```

Contenido:

```json
{
  "online": true,
  "thermo": "stable",
  "photo": false,
  "hr": 124,
  "skin": 2,
  "awake": false,
  "baby": true
}
```

| Campo    | Tipo   | Valores                                     | Efecto en el panel                                        |
| -------- | ------ | ------------------------------------------- | --------------------------------------------------------- |
| `online` | bool   | `true`/`false`                              | Chip *Conexión*; si `false` todo pasa a "Sin datos"       |
| `thermo` | string | `off` \| `heating` \| `stable` \| `alarm`   | Chip *Calor* + halo ámbar (heating) o rojo pulsante (alarm) |
| `photo`  | bool   | `true`/`false`                              | Chip *Luz* + halo azul de fototerapia                      |
| `hr`     | number | bpm; `0` = sin sensor                       | Corazón animado late a ese ritmo (no se muestra el número) |
| `skin`   | number | `0`–`5` (claro → oscuro)                    | Tono de piel del bebé (cambio instantáneo de paleta)       |
| `awake`  | bool   | `true`/`false`                              | Bebé despierto (ojos abiertos) o dormido (Zzz)             |
| `baby`   | bool   | `true`/`false` (default `true` si no se envía) | Muestra/oculta al bebé. En el flanco `false → true` estando online: fanfarria retro por el buzzer + 10 s de latido audible. Lo calcula la rulechain: termorregulación ∨ fototerapia ∨ pulso. Si `false`, el panel muestra la incubadora vacía. |

Si el nodo `state` **no existe** en Firebase, el panel lo interpreta como
"Sin IncuNest vinculada" (aún no hay incubadora asignada a este panel).

Cualquier actualización parcial (`patch` o `put` de un solo campo, p. ej.
`/incutwin/incunest-001/state/thermo = "heating"`) también se aplica.

Si no llega ningún evento en `DATA_STALE_S` segundos (90 por defecto), el
panel marca la incubadora como sin conexión.

## Puente ThingsBoard → Firebase

Opciones (elige una):

**A. Rule chain + REST API call node (recomendado si autogestionas TB)**
1. En la rule chain raíz del dispositivo IncuNest, añade un nodo
   *transformation → script* que mapee la telemetría al esquema anterior.
2. Encadena un nodo *external → REST API call*:
   - Método: `PATCH`
   - URL: `https://<proyecto>.firebasedatabase.app/incutwin/incunest-001/state.json?auth=<secret>`
   - Body: `${msg}`
3. Para el atributo compartido `skin` (tono de piel), añade la misma cadena
   al evento *Attributes updated*.

**B. Cloud Function / servicio intermedio**
Un servicio (Cloud Function programada o worker) consulta la API REST de
ThingsBoard (o se suscribe por WebSocket) y escribe el nodo de Firebase.
Útil si no puedes tocar las rule chains.

Ejemplo de transformación (script del nodo TB):

```javascript
var out = {
  online: true,
  thermo: msg.heater_on ? (msg.temp_stable ? "stable" : "heating") : "off",
  photo: msg.phototherapy === true,
  hr: msg.heart_rate || 0,
  baby: (msg.heater_on === true) || (msg.phototherapy === true) ||
        ((msg.heart_rate || 0) > 0)
};
return {msg: out, metadata: metadata, msgType: msgType};
```

## Seguridad

- Crea un token de solo lectura para el panel (regla `.read` limitada a
  `/incutwin/$device`) y ponlo en `FIREBASE_AUTH`.
- El firmware usa TLS con `setInsecure()` (no valida la CA). Para
  producción, fija la CA raíz de Google en `firebase_stream.cpp`.

## Probar sin incubadora

```bash
curl -X PUT \
  "https://<proyecto>.firebasedatabase.app/incutwin/incunest-001/state.json?auth=<secret>" \
  -d '{"online":true,"thermo":"heating","photo":true,"hr":130,"skin":3,"awake":true,"baby":true}'
```

El panel debe reaccionar en menos de un segundo.
