# Configurar Firebase para IncuTwin

Guía paso a paso para crear el proyecto de Firebase que alimenta los
paneles IncuTwin. Solo se usa **Realtime Database** (RTDB); el panel se
suscribe por streaming SSE, que es muy ligero para el ESP32.

## 1. Crear el proyecto

1. Entra en [console.firebase.google.com](https://console.firebase.google.com)
   con la cuenta de Google de Medical Open World.
2. **Añadir proyecto** → nombre `incutwin` (o similar). No hace falta
   Google Analytics.
3. Cuando termine, entra en el proyecto.

## 2. Crear la Realtime Database

1. Menú izquierdo → **Compilación → Realtime Database** → *Crear base de
   datos*.
2. Ubicación: **Bélgica (europe-west1)** (donantes principalmente en
   Europa; requisito RGPD de datos en la UE).
3. Modo: empieza en **modo bloqueado** (las reglas se ponen en el paso 4).
4. Apunta la URL resultante, p. ej.:
   `https://incutwin-default-rtdb.europe-west1.firebasedatabase.app`
   El host (sin `https://`) va en `FIREBASE_HOST` de `include/config.h`.

## 3. Estructura de datos

```
/incutwin/
  {SN}/                      p.ej. ITW-2607-0042
    state/                   ← lo LEE el panel (streaming)
      online: true
      thermo: "stable"
      photo: false
      hr: 124
      skin: 2                ← según el país de la IncuNest asignada
      awake: false
    owner/                   ← lo escribe la app al vincular
      uid: "..."
      name: "María"
      email: "maria@..."
    pairing/
      code: "483920"         ← código del QR de onboarding (verificación)
```

El campo `skin` (0–5, claro → oscuro) lo fija el backend según el país
donde esté desplegada la IncuNest apadrinada (mapa país → tono en el
puente; ver `docs/FIREBASE_SCHEMA.md`).

## 4. Reglas de seguridad

Realtime Database → pestaña **Reglas**:

```json
{
  "rules": {
    "incutwin": {
      "$sn": {
        "state": {
          ".read": "auth != null",
          ".write": "auth != null && auth.token.bridge === true"
        },
        "owner": {
          ".read": "auth != null && auth.uid === data.child('uid').val()",
          ".write": "auth != null && !data.exists()"
        },
        "pairing": {
          ".read": false,
          ".write": "auth != null && auth.token.bridge === true"
        }
      }
    }
  }
}
```

Notas:
- `bridge === true` es un *custom claim* que se asigna a la cuenta de
  servicio del puente ThingsBoard→Firebase.
- Para la fase de pruebas puedes usar temporalmente
  `".read": true` en `state` y dejar `FIREBASE_AUTH` vacío en el panel,
  pero **no lo dejes así en producción**.

## 5. Credenciales para el panel

Dos opciones, de más simple a más segura:

**A. Database secret (legado, suficiente para el piloto)**
Configuración del proyecto (⚙) → *Cuentas de servicio* → *Secretos de la
base de datos* → copia el secreto y ponlo en `FIREBASE_AUTH`. Vale para
todos los paneles.

**B. Token por dispositivo (producción)**
El puente genera un *custom token* por SN con claim `{"sn": "ITW-..."}`
y regla `".read": "auth.token.sn === $sn"`. Así cada panel solo puede
leer su propio nodo. Se puede entregar el token durante el onboarding
(respuesta de provisión de ThingsBoard como atributo) o precargarlo en
fabricación.

## 6. Cuenta de servicio para el puente

1. Configuración del proyecto → **Cuentas de servicio** → *Generar nueva
   clave privada* (JSON).
2. Esa clave la usa el puente (rule chain de ThingsBoard con nodo REST, o
   Cloud Function) para escribir `/incutwin/{SN}/state`.
3. Si usas el nodo REST de ThingsBoard sin SDK de Firebase: usa el
   database secret en la URL (`...state.json?auth=<secret>`), que no
   necesita OAuth.

## 7. Probar

```bash
curl -X PUT \
  "https://<proyecto>.europe-west1.firebasedatabase.app/incutwin/ITW-TEST-0001/state.json?auth=<secret>" \
  -d '{"online":true,"thermo":"heating","photo":true,"hr":130,"skin":3,"awake":true}'
```

Con un panel configurado con ese SN de prueba, la reacción debe ser
inmediata (halo ámbar, lámpara encendida, corazón a 130 bpm, tono 3).

## 8. RGPD

- El email y nombre del donante se guardan en `/owner` — pide
  consentimiento explícito (la casilla del portal de onboarding lo hace).
- Firma un DPA con Google (automático en Firebase, verifica en
  *Configuración → Privacidad*).
- Ofrece borrado: al desvincular desde la app, borra `/owner` y las
  telemetrías asociadas en ThingsBoard.
