# Sistema de notificaciones con buzzer — diseño

**Fecha:** 2026-07-21
**Estado:** aprobado

## Objetivo

Añadir sonido al gemelo digital IncuTwin (CrowPanel Advance 2.8") con el
buzzer de a bordo, en estilo retro 8-bit:

1. **Jingle de arranque** al iniciar IncuTwin.
2. **Fanfarria de "bebé detectado"** cuando la rulechain de ThingsBoard
   marque `baby_inside = true` (termorregulación ∨ fototerapia ∨ pulso).
3. **Latido de corazón audible** ("lub-dub" al bpm real):
   - durante **10 s** tras la notificación de bebé detectado (el corazón
     visual sigue latiendo siempre, como hasta ahora);
   - mientras el usuario **mantiene el dedo sobre el bebé** ("Agarra mi
     mano").
4. **Toggle Sonido on/off** en Ajustes, persistente (NVS), por defecto ON.

## Hardware

- Buzzer en **GPIO8**, control por PWM. Confirmado en el firmware de
  fábrica de Elecrow (`buzzer_test()` usa `analogWrite(8, …)`).
- Se define `BUZZER_PIN 8` en `include/pins_config.h`.
- Tonos con LEDC: **canal 2**. El backlight usa el canal 0 y en ESP32 los
  canales 0–1 comparten timer; el canal 2 usa otro timer, así que cambiar
  la frecuencia del buzzer no altera el PWM del backlight.
- El `pinMode(18, OUTPUT)` existente en `main.cpp` NO es el buzzer (GPIO18
  es el conmutador del módulo inalámbrico/altavoz en el código de fábrica);
  no se toca.

## Arquitectura

Módulo nuevo **`src/sound/sound.h` / `src/sound/sound.cpp`**, secuenciador
no bloqueante sobre un `lv_timer` de ~15 ms. Todo corre en el hilo de UI
(donde ya viven los eventos táctiles y el refresco de estado): sin tarea
FreeRTOS nueva y sin locks nuevos.

### API pública

```c
void sound_init(void);                       /* LEDC + NVS + lv_timer     */
void sound_set_enabled(bool on);             /* persiste en NVS "sound"   */
bool sound_is_enabled(void);
void sound_play_boot(void);                  /* jingle de arranque        */
void sound_on_state(const twin_state_t *st); /* flanco baby_inside + bpm  */
void sound_hand_hold(bool holding);          /* dedo sobre el bebé        */
```

### Comportamiento interno

- **Secuenciador**: melodía = array de `{freq_hz, dur_ms}` (freq 0 =
  silencio). El `lv_timer` avanza la nota actual con
  `ledcWriteTone(canal 2, freq)`; al terminar, silencio
  (`ledcWriteTone(…, 0)`).
- **Latido**: activo si (ventana de 10 s abierta ∨ dedo sobre el bebé) ∧
  `bpm > 0` ∧ sonido habilitado. Patrón lub-dub repetido con periodo
  `60000 / bpm` ms. Una melodía puntual (fanfarria) tiene prioridad; el
  latido se reanuda al acabar.
- **Flanco de detección**: `sound_on_state()` compara `baby_inside` con el
  valor anterior; en `false → true` dispara la fanfarria y abre la ventana
  de 10 s. También cachea `heart_rate` para el patrón.
- **Silenciado**: `sound_set_enabled(false)` corta inmediatamente cualquier
  sonido en curso y bloquea los siguientes.

## Frecuencias (retro 8-bit, onda cuadrada)

| Evento | Notas | Timing |
|---|---|---|
| Arranque | C5–E5–G5–C6 (523, 659, 784, 1047 Hz) | 70 ms/nota, última 180 ms |
| Bebé detectado | G5–C6–E6 (784, 1047, 1319 Hz) | 90 / 90 / 220 ms |
| Latido | lub E4 (330 Hz) 50 ms · silencio 110 ms · dub C4 (262 Hz) 40 ms · silencio hasta `60000/bpm` | sincronizado al bpm real |

Contingencia: los buzzers magnéticos pequeños reproducen mal < ~250 Hz. Si
en hardware el lub-dub suena demasiado flojo, subirlo una octava
(E5 659 / C5 523). Queda como paso de verificación manual.

## Detección de bebé: campo `baby` existente

*(Revisado 2026-07-21: el firmware ya incorporó `baby_present` — clave
Firebase `"baby"`, default `true` — junto con `node_seen` y la vista
"sin bebé". Decisión del usuario: unificar en ese campo en vez de añadir
`baby_inside`.)*

- No se añade ningún campo nuevo: el disparador es el flanco de subida de
  `online ∧ baby_present`, exactamente la misma condición `show_baby` que
  usa la UI (con `online = wifi ∧ cloud ∧ node_seen ∧ incubator_online ∧
  !stale`). Sonido y pantalla no pueden divergir.
- La rulechain de ThingsBoard calcula `baby` (termorregulación ∨
  fototerapia ∨ pulso) y lo escribe en `/incutwin/{SN}/state/baby`.
- Nota: `baby_present` es `true` por defecto (bridges que no envían
  `baby` siguen funcionando), así que en esos casos la notificación suena
  cuando el panel pasa a mostrar al bebé (transición a online). Asumido.
- `docs/FIREBASE_SCHEMA.md`: documentar el cálculo de `baby` en la
  rulechain y el efecto sonoro del flanco.

## UI de Ajustes

- Fila "Sonido" con `lv_switch` bajo los swatches de tono de piel
  (~y 240, entre el hint y el footer).
- i18n: `STR_SOUND` = "Sonido" / "Sound".
- El switch refleja `sound_is_enabled()` y llama a `sound_set_enabled()`.
- `update_texts()` refresca la etiqueta al cambiar de idioma.

## Puntos de integración

| Dónde | Qué |
|---|---|
| `main.cpp::setup()` | `sound_init()` tras `lv_init()`; `sound_play_boot()` al final del arranque (ambas ramas: onboarding y normal) |
| `ui_main.cpp` refresco de estado | `sound_on_state(&st)` donde ya se aplican los cambios de `g_state` |
| `ui_main.cpp::baby_touched()` | `sound_hand_hold(true)` en PRESSED, `sound_hand_hold(false)` en RELEASED/PRESS_LOST |
| `ui_main.cpp::build_settings()` | fila Sonido + switch |

## Manejo de errores

- `bpm == 0` → no hay latido audible (ni en ventana ni al tocar).
- Melodía solicitada con sonido deshabilitado → no-op.
- Ventana de 10 s con `millis()` (uint32, comparación con resta: seguro
  ante overflow).

## Verificación (manual — este repo no tiene entorno de test nativo)

1. Arranque → jingle C5–E5–G5–C6 una sola vez.
2. `curl` PATCH con `{"baby_inside":true}` → fanfarria + latido audible
   ~10 s; el corazón visual sigue después.
3. `baby_inside` se mantiene `true` → sin re-notificación; `false → true`
   de nuevo → re-notifica.
4. Mantener el dedo sobre el bebé → latido mientras dura el toque
   (con `hr > 0`).
5. Ajustes → Sonido OFF → silencio total (arranque incluido tras reboot);
   el ajuste sobrevive al reinicio.
6. Backlight estable (sin parpadeo) mientras suena el buzzer.
7. Volumen del lub-dub aceptable; si no, subir una octava.

## Fuera de alcance

- Sonido de alarma para `THERMO_ALARM` (posible siguiente iteración).
- Control de volumen (el buzzer PWM solo da duty fijo razonable).
- Uso del altavoz I2S (SPK) — el buzzer basta para notificaciones.
