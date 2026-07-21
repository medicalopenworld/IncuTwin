# SIM_MODE — Modo simulación embebido (diseño)

**Fecha:** 2026-07-21
**Estado:** aprobado (pendiente de plan de implementación)

## Objetivo

Poder simular todos los estados de IncuTwin en el **panel físico real**
(CrowPanel Advance 2.8") sin necesitar una IncuNest, ThingsBoard ni
Firebase: un servidor web embebido en el propio ESP32-S3 desde el que se
eligen escenarios predefinidos, se fijan campos individuales o se lanza
una secuencia automática de demo.

Usos principales: desarrollo de UI y demos/ferias.

## Activación

- Solo por compilación. Nuevo entorno PlatformIO
  `[env:crowpanel_advance_28_sim]` en `platformio.ini` que hereda de
  `[env:crowpanel_advance_28]` y añade `-D SIM_MODE`.
- El binario de producción (`crowpanel_advance_28`) nunca incluye el
  código de simulación: todo el módulo va detrás de `#ifdef SIM_MODE`.
- Motivo de no usar un `#define` manual en `config.h`: evita commitear
  el flag activado por accidente.

## Arquitectura

```
Navegador (móvil/PC en la misma WiFi)
        │ HTTP
        ▼
sim_server (WebServer:80, tarea FreeRTOS core 0)
        │ state_lock()/state_unlock() + g_state_dirty
        ▼
g_state ──> UI (core 1, igual que en producción)
```

- Nuevo módulo `src/net/sim_server.{h,cpp}`, calcado del patrón de
  `src/net/portal.cpp` (`WebServer` síncrono + tarea propia).
- Con `SIM_MODE`, `main.cpp` **no** llama a `firebase_stream_start()`:
  el simulador es la única fuente que escribe `g_state`. No hay
  carreras entre datos reales y simulados.
- `wifi_service_start()` sigue arrancando igual; `tb_client_start()`
  **no arranca en SIM_MODE**: evita provisión falsa de ThingsBoard,
  telemetría contaminada y riesgo de auto-flashearse por OTA en una demo.
- El panel imprime su IP por serie y la muestra en Ajustes (ya existe
  `WiFi.localIP()` en la pantalla de Ajustes).
- En SIM_MODE, `sim_server` fija al arrancar un estado inicial sano:
  `cloud_connected=true`, `node_seen=true`, `incubator_online=true`,
  escenario "Bebé dormido", y refresca `last_update_ms` mientras el
  modo esté activo para que el watchdog `DATA_STALE_S` no marque la
  incubadora como desconectada.

## Página web de control

Una sola página HTML embebida (string en PROGMEM, sin dependencias
externas, como el portal cautivo). Tres bloques:

1. **Escenarios predefinidos** (botones de un toque):
   - Bebé dormido — `online:true, thermo:stable, photo:false, hr:120, awake:false, baby:true`
   - Bebé despierto — igual pero `awake:true, hr:140`
   - Calentando — `thermo:heating`
   - Alarma — `thermo:alarm, hr:180, awake:true`
   - Fototerapia — `photo:true`
   - Incubadora vacía — `baby:false`
   - IncuNest apagada — `online:false`
   - Sin vincular — `node_seen:false`
2. **Control campo a campo**: selector `thermo`
   (off/heating/stable/alarm), checkboxes `photo/awake/baby/online`,
   sliders `hr` (0–220) y `skin` (0–5). Botón "Aplicar" (envía solo lo
   modificado o el conjunto completo; PATCH semántico).
3. **Modo demo**: Start/Stop de una secuencia que recorre los
   escenarios en bucle cada N segundos (10 s por defecto,
   configurable). El bucle corre **en el firmware** (timer/tarea), no
   en el navegador, para sobrevivir al cierre de la pestaña.

## API HTTP

| Método | Ruta     | Cuerpo / respuesta                                        |
| ------ | -------- | --------------------------------------------------------- |
| GET    | `/`      | Página HTML de control                                     |
| GET    | `/state` | JSON con el estado actual (para refrescar la página)       |
| POST   | `/state` | JSON parcial con el mismo esquema que Firebase (`{"thermo":"alarm","hr":180}`); aplica solo los campos presentes |
| POST   | `/demo`  | `{"run":true,"interval_s":10}` — arranca/para la secuencia |

El parseo de `POST /state` reutiliza la misma semántica que
`apply_field()` de `firebase_stream.cpp` (mismos nombres y valores de
campo que el nodo Firebase: `online`, `thermo`, `photo`, `hr`, `skin`,
`awake`, `baby`), más un campo extra `linked` (bool) para simular
`node_seen` (no existe en el esquema Firebase, donde equivale a la
ausencia del nodo).

## Manejo de errores

- JSON inválido en `POST` → HTTP 400 con mensaje breve.
- Valores fuera de rango: `skin` se ignora fuera de 0–5 (igual que en
  `firebase_stream`), `hr` se satura a 0–300, `thermo` desconocido → `off`.
- Cuerpo mayor que el buffer JSON (768 B, como el stream) → 400.

## Pruebas / verificación

No hay entorno de test nativo para `src/net` (requiere WiFi/WebServer):
verificación manual documentada:

1. Compilar y flashear `crowpanel_advance_28_sim`; comprobar que
   `crowpanel_advance_28` sigue compilando sin el módulo.
2. Abrir `http://<ip-del-panel>/` desde el móvil y recorrer los 8
   escenarios comprobando la reacción de la UI (< 1 s).
3. Fijar campos individuales (hr, skin) y verificar el efecto.
4. Lanzar el modo demo, cerrar la pestaña y comprobar que sigue rotando.
5. `curl -X POST` con JSON inválido → 400, y el panel no se ve afectado.

## Fuera de alcance

- Simulación en PC sin hardware (build nativo/SDL): descartado en esta
  iteración por decisión del usuario.
- Activación en runtime desde Ajustes: descartada; solo compile-time.
- Autenticación del servidor web: es un build de desarrollo que nunca
  se distribuye; el servidor solo existe en `_sim`.
