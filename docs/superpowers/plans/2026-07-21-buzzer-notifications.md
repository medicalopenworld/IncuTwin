# Buzzer Notifications Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sonido retro 8-bit con el buzzer de a bordo (GPIO8): jingle de arranque, fanfarria al detectar bebé (`baby_inside`), latido "lub-dub" audible (10 s tras detección + mientras se toca al bebé) y toggle Sonido en Ajustes.

**Architecture:** Módulo nuevo `src/sound/` con secuenciador no bloqueante sobre `lv_timer` (~15 ms) y tonos LEDC en el canal 2 (el backlight usa el 0; los canales 0–1 comparten timer, el 2 no). Eventos disparados desde el hilo de UI: `setup()` (boot), `ui_apply_state()` (flanco de `baby_inside` + bpm) y `baby_touched()` (mantener el dedo). Estado nuevo `baby_inside` parseado del stream SSE de Firebase.

**Tech Stack:** PlatformIO + Arduino-ESP32 (LEDC), LVGL 8.3, Preferences (NVS), ArduinoJson.

**Spec:** `docs/superpowers/specs/2026-07-21-buzzer-notifications-design.md`

> **Revisión 2026-07-21 (durante ejecución):** el firmware incorporó en
> paralelo `baby_present` (clave Firebase `"baby"`, default `true`),
> `node_seen` y la vista "sin bebé". Decisión del usuario: **unificar en
> `baby`** — no existe `baby_inside`. El disparador sonoro es el flanco de
> subida de `online ∧ baby_present` (idéntico al `show_baby` de la UI, con
> `online` incluyendo `node_seen`). Las tareas de abajo ya reflejan esto.

## Global Constraints

- Buzzer: **GPIO8**, LEDC **canal 2**. No tocar el canal 0 (backlight) ni el `pinMode(18, OUTPUT)` existente.
- Este directorio **no es un repo git**: no hay pasos de commit. La puerta de verificación de cada tarea es `pio run -e crowpanel_advance_28` sin errores.
- No hay entorno de test nativo en este repo: verificación por compilación + checklist manual en hardware (Tarea 5).
- Estilo del código: como el existente — comentarios escasos en inglés, headers con `extern "C"`, prefijo `s_` para estáticos, 4 espacios.
- Textos de UI vía i18n (`tr()`), cortos, ES/EN.
- Frecuencias exactas de la spec: boot C5–E5–G5–C6 (523/659/784/1047 Hz, 70/70/70/180 ms); bebé G5–C6–E6 (784/1047/1319 Hz, 90/90/220 ms); latido lub 330 Hz 50 ms · silencio 110 ms · dub 262 Hz 40 ms · silencio hasta `60000/bpm`.

---

### Task 1: Módulo de sonido + jingle de arranque

**Files:**
- Modify: `include/pins_config.h` (añadir al final)
- Modify: `src/app/app_state.h` (struct `twin_state_t`)
- Create: `src/sound/sound.h`
- Create: `src/sound/sound.cpp`
- Modify: `src/main.cpp` (include + `setup()`)

**Interfaces:**
- Produces (lo usan las Tareas 2–3):
  - `void sound_init(void);`
  - `void sound_set_enabled(bool on);` / `bool sound_is_enabled(void);`
  - `void sound_play_boot(void);`
  - `void sound_on_state(const twin_state_t *st);`
  - `void sound_hand_hold(bool holding);`
  - campo `bool baby_inside;` en `twin_state_t` (la Tarea 2 lo alimenta
    desde Firebase)

- [ ] **Step 1: Añadir el pin del buzzer a `include/pins_config.h`**

Al final del fichero:

```c
/* On-board buzzer (PWM; factory firmware drives it with analogWrite(8, x)) */
#define BUZZER_PIN 8
```

- [ ] **Step 1b: (eliminado en la revisión)** El estado ya tiene
`baby_present` (clave `"baby"`, parseada en `firebase_stream.cpp`); no se
añade ningún campo.

- [ ] **Step 2: Crear `src/sound/sound.h`**

```c
#pragma once

/* Retro (8-bit style) buzzer notifications on the on-board buzzer.
 *
 * Non-blocking: a ~15 ms lv_timer steps through {freq, dur} sequences
 * with LEDC tones. Everything runs on the UI thread; call these only
 * from setup()/LVGL callbacks.
 *
 *   - boot jingle
 *   - "baby detected" fanfare on the (online && baby_present) rising
 *     edge — the same show_baby condition the UI uses — plus an
 *     audible heartbeat window of 10 s (the on-screen heart keeps
 *     beating on its own, always)
 *   - audible heartbeat while the baby is being touched
 *   - on/off toggle persisted in NVS ("incutwin"/"sound", default on)
 */

#include <stdbool.h>

#include "app/app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

void sound_init(void);   /* LEDC channel + NVS + lv_timer */
void sound_set_enabled(bool on);
bool sound_is_enabled(void);

void sound_play_boot(void);
void sound_on_state(const twin_state_t *st); /* edge detect + bpm cache */
void sound_hand_hold(bool holding);          /* finger on the baby      */

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 3: Crear `src/sound/sound.cpp`**

```cpp
#include "sound.h"

#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>

#include "config.h"      /* DATA_STALE_S */
#include "pins_config.h"

#define BUZZER_LEDC_CH 2 /* backlight owns ch 0; 0-1 share a timer */

typedef struct {
    uint16_t freq;   /* Hz, 0 = rest */
    uint16_t dur_ms;
} note_t;

/* retro square-wave jingles (equal temperament) */
static const note_t BOOT_MELODY[] = {
    {523, 70}, {659, 70}, {784, 70}, {1047, 180}}; /* C5 E5 G5 C6 */
static const note_t BABY_MELODY[] = {
    {784, 90}, {1047, 90}, {1319, 220}};           /* G5 C6 E6    */

/* heartbeat "lub-dub" (E4/C4; one octave up if too quiet on hardware) */
#define HB_LUB_HZ 330
#define HB_LUB_MS 50
#define HB_GAP_MS 110
#define HB_DUB_HZ 262
#define HB_DUB_MS 40
#define HB_WINDOW_MS 10000 /* audible heartbeat after baby detected */

static bool s_enabled = true;

/* one-shot melody being played (nullptr = none) */
static const note_t *s_seq = nullptr;
static uint8_t s_seq_len = 0, s_seq_idx = 0;
static uint32_t s_note_end = 0;

/* heartbeat state */
static uint16_t s_bpm = 0;
static bool s_holding = false;
static uint32_t s_hb_until = 0;   /* window deadline (0 = closed)  */
static uint8_t s_hb_phase = 0;    /* 0 lub, 1 gap, 2 dub, 3 rest   */
static uint32_t s_hb_next = 0;
static bool s_prev_inside = false;

static void tone_out(uint16_t freq) { ledcWriteTone(BUZZER_LEDC_CH, freq); }

static bool hb_active(void) {
    if (!s_enabled || s_bpm == 0) return false;
    if (s_holding) return true;
    return s_hb_until != 0 && (int32_t)(millis() - s_hb_until) < 0;
}

static void seq_start(const note_t *seq, uint8_t len) {
    if (!s_enabled) return;
    s_seq = seq;
    s_seq_len = len;
    s_seq_idx = 0;
    s_note_end = millis() + seq[0].dur_ms;
    tone_out(seq[0].freq);
}

static void seq_stop(void) {
    s_seq = nullptr;
    tone_out(0);
}

static void hb_step(uint32_t now) {
    if ((int32_t)(now - s_hb_next) < 0) return;
    uint32_t period = 60000UL / s_bpm;
    switch (s_hb_phase) {
        case 0:
            tone_out(HB_LUB_HZ);
            s_hb_next = now + HB_LUB_MS;
            break;
        case 1:
            tone_out(0);
            s_hb_next = now + HB_GAP_MS;
            break;
        case 2:
            tone_out(HB_DUB_HZ);
            s_hb_next = now + HB_DUB_MS;
            break;
        default: { /* rest until the beat period is complete */
            tone_out(0);
            uint32_t used = HB_LUB_MS + HB_GAP_MS + HB_DUB_MS;
            s_hb_next = now + (period > used + 50 ? period - used : 50);
            break;
        }
    }
    s_hb_phase = (s_hb_phase + 1) & 3;
}

static void sound_tick(lv_timer_t *t) {
    (void)t;
    uint32_t now = millis();

    if (s_seq) { /* a one-shot melody has priority */
        if ((int32_t)(now - s_note_end) < 0) return;
        s_seq_idx++;
        if (s_seq_idx >= s_seq_len) {
            seq_stop();
            s_hb_phase = 0; /* resume heartbeat from a clean lub */
            s_hb_next = now;
        } else {
            tone_out(s_seq[s_seq_idx].freq);
            s_note_end = now + s_seq[s_seq_idx].dur_ms;
        }
        return;
    }

    if (hb_active()) {
        hb_step(now);
    } else if (s_hb_phase != 0) { /* just deactivated: silence output */
        s_hb_phase = 0;
        s_hb_next = 0;
        tone_out(0);
    }
}

void sound_init(void) {
    ledcSetup(BUZZER_LEDC_CH, 2000, 10);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CH);
    tone_out(0);

    Preferences p;
    p.begin("incutwin", true);
    s_enabled = p.getBool("sound", true);
    p.end();

    lv_timer_create(sound_tick, 15, nullptr);
}

void sound_set_enabled(bool on) {
    if (on == s_enabled) return;
    s_enabled = on;
    Preferences p;
    p.begin("incutwin", false);
    p.putBool("sound", on);
    p.end();
    if (!on) { /* cut anything currently playing */
        seq_stop();
        s_hb_until = 0;
        s_hb_phase = 0;
    }
}

bool sound_is_enabled(void) { return s_enabled; }

void sound_play_boot(void) {
    seq_start(BOOT_MELODY, sizeof(BOOT_MELODY) / sizeof(BOOT_MELODY[0]));
}

void sound_on_state(const twin_state_t *st) {
    bool stale = st->last_update_ms == 0 ||
                 (millis() - st->last_update_ms) > (DATA_STALE_S * 1000UL);
    /* same definition ui_apply_state() uses for show_baby */
    bool online = st->wifi_connected && st->cloud_connected &&
                  st->node_seen && st->incubator_online && !stale;

    s_bpm = online ? st->heart_rate : 0; /* mirror the on-screen heart */

    bool inside = online && st->baby_present;
    if (inside && !s_prev_inside) {
        seq_start(BABY_MELODY, sizeof(BABY_MELODY) / sizeof(BABY_MELODY[0]));
        s_hb_until = millis() + HB_WINDOW_MS;
        if (s_hb_until == 0) s_hb_until = 1; /* 0 means "closed" */
    }
    s_prev_inside = inside;
}

void sound_hand_hold(bool holding) { s_holding = holding; }
```

- [ ] **Step 4: Integrar en `src/main.cpp`**

Añadir el include junto a los demás de `src/`:

```cpp
#include "sound/sound.h"
```

En `setup()`, justo después del bloque `lv_indev_drv_register(&indev_drv);`:

```cpp
sound_init();
```

Y al final de `setup()`, después del `if/else` de onboarding (suena en ambas ramas):

```cpp
sound_play_boot();
```

- [ ] **Step 5: Compilar**

Run: `pio run -e crowpanel_advance_28`
Expected: `SUCCESS`, sin warnings nuevos en `src/sound/`.

---

### Task 2: Eventos de sonido en la UI

**Files:**
- Modify: `src/ui/ui_main.cpp` (`baby_touched`, `ui_apply_state`, includes)

**Interfaces:**
- Consumes: `sound_on_state(const twin_state_t*)`, `sound_hand_hold(bool)` (Tarea 1). El parseo de `"baby"` → `baby_present` ya existe en `firebase_stream.cpp` — no tocar.

- [ ] **Step 1: Disparar eventos de sonido en `src/ui/ui_main.cpp`**

Añadir el include junto a `#include "baby_widget.h"`:

```cpp
#include "sound/sound.h"
```

En `baby_touched()` (línea ~145), añadir las llamadas junto a `usage_hand_*`:

```cpp
static void baby_touched(lv_event_t *e) {
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_PRESSED) {
        usage_hand_begin();
        sound_hand_hold(true);
        baby_set_awake(true);
    } else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) {
        usage_hand_end();
        sound_hand_hold(false);
        state_lock();
        bool awake = g_state.awake;
        state_unlock();
        baby_set_awake(awake);
    }
}
```

En `ui_apply_state()`, tras `home_set_view(show_baby);`:

```cpp
    sound_on_state(&st);
```

- [ ] **Step 2: Compilar**

Run: `pio run -e crowpanel_advance_28`
Expected: `SUCCESS`.

---

### Task 3: Toggle "Sonido" en Ajustes + i18n

**Files:**
- Modify: `src/ui/i18n.h` (enum) y `src/ui/i18n.cpp` (tabla)
- Modify: `src/ui/ui_main.cpp` (`build_settings`, `update_texts`, estáticos)

**Interfaces:**
- Consumes: `sound_is_enabled()`, `sound_set_enabled(bool)` (Tarea 1).

- [ ] **Step 1: Añadir `STR_SOUND` a i18n**

`src/ui/i18n.h` — en el enum, tras `STR_BEATING,`:

```c
    STR_SOUND,
```

`src/ui/i18n.cpp` — en `STRINGS`, tras la fila `STR_BEATING` (el orden debe
coincidir con el enum):

```c
    /* STR_SOUND         */ {"Sonido", "Sound"},
```

- [ ] **Step 2: Añadir la fila Sonido en `build_settings()` (`src/ui/ui_main.cpp`)**

Junto a los otros estáticos de settings (línea ~46):

```cpp
static lv_obj_t *s_lbl_sound, *s_sw_sound;
```

Callback, junto a `lang_clicked` (zona de idioma, línea ~168):

```cpp
static void sound_switch_changed(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    sound_set_enabled(lv_obj_has_state(sw, LV_STATE_CHECKED));
}
```

En `build_settings()`, entre el bloque de `s_lbl_hint` y el del footer
(`s_lbl_wifi`):

```cpp
    /* sound on/off */
    s_lbl_sound = lv_label_create(scr_settings);
    lv_label_set_text(s_lbl_sound, tr(STR_SOUND));
    lv_obj_set_style_text_color(s_lbl_sound, COL_NAVY_DARK, 0);
    lv_obj_set_pos(s_lbl_sound, 12, 248);

    s_sw_sound = lv_switch_create(scr_settings);
    lv_obj_set_size(s_sw_sound, 56, 30);
    lv_obj_set_pos(s_sw_sound, 172, 242);
    lv_obj_set_style_bg_color(s_sw_sound, COL_NAVY, LV_PART_INDICATOR |
                                                        LV_STATE_CHECKED);
    if (sound_is_enabled()) lv_obj_add_state(s_sw_sound, LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_sw_sound, sound_switch_changed,
                        LV_EVENT_VALUE_CHANGED, nullptr);
```

- [ ] **Step 3: Refrescar la etiqueta al cambiar de idioma**

En `update_texts()` (línea ~382), junto a las demás `lv_label_set_text`:

```cpp
    lv_label_set_text(s_lbl_sound, tr(STR_SOUND));
```

- [ ] **Step 4: Compilar**

Run: `pio run -e crowpanel_advance_28`
Expected: `SUCCESS`.

---

### Task 4: Documentación (`FIREBASE_SCHEMA.md` + `README.md`)

**Files:**
- Modify: `docs/FIREBASE_SCHEMA.md` (JSON de ejemplo ~línea 25, tabla ~36,
  snippet de rulechain ~72, curl ~92 — Read el fichero primero; las líneas
  pueden variar)
- Modify: `README.md` (lista de características)

- [ ] **Step 1: Documentar `baby` y su efecto sonoro en el esquema**

Read `docs/FIREBASE_SCHEMA.md` primero: la revisión paralela puede haber
documentado ya el campo `baby`. Asegurar que quede así (añadiendo solo lo
que falte, sin duplicar):

Si el JSON de ejemplo del nodo no tiene `baby`, añadir tras `"hr": 124,`:

```json
  "baby": true,
```

En la tabla de campos, fila de `baby` (crearla si no existe; si existe,
ampliar su descripción con el efecto sonoro):

```markdown
| `baby` | bool | `true`/`false` (default `true` si no se envía) | Muestra/oculta al bebé. En el flanco `false → true` estando online: fanfarria retro por el buzzer + 10 s de latido audible. Lo calcula la rulechain: termorregulación ∨ fototerapia ∨ pulso |
```

En el snippet de rulechain (el objeto que se escribe en Firebase), tras
`hr: msg.heart_rate || 0`, añadir si no existe:

```javascript
  baby: (msg.heater_on === true) || (msg.phototherapy === true) ||
        ((msg.heart_rate || 0) > 0)
```

En el `curl` de prueba, incluir el campo en el JSON si no está:

```
"baby":true
```

- [ ] **Step 2: Añadir la característica al `README.md`**

En la lista de características, tras el bullet de "Efectos de iluminación
en pantalla":

```markdown
- **Notificaciones retro por buzzer**: jingle 8-bit al arrancar, fanfarria
  cuando la incubadora detecta al bebé (campo `baby`) con 10 s de latido
  audible, y "lub-dub" al bpm real mientras tocas al bebé. Silenciable
  desde Ajustes (persistente).
```

- [ ] **Step 3: Revisar coherencia**

Releer las dos secciones editadas: el nombre del campo es `baby_inside` en
todos los sitios (JSON, tabla, rulechain, curl) y el pin/canal no se
menciona en docs de datos.

---

### Task 5: Verificación manual en hardware

**Files:** ninguno (checklist).

- [ ] **Step 1: Flashear**

Run: `pio run -t upload -e crowpanel_advance_28 && pio device monitor`
Expected: arranca y por serie sale `IncuTwin <ver> (SN …) ready`.

- [ ] **Step 2: Jingle de arranque**

Al encender: arpegio C5–E5–G5–C6 una sola vez. El backlight no parpadea.

- [ ] **Step 3: Bebé detectado (usar el curl de FIREBASE_SCHEMA.md)**

PATCH con `{"baby":true,"hr":130,"online":true}` → fanfarria
G5–C6–E6 + latido audible ~10 s que luego calla; el corazón visual sigue.

- [ ] **Step 4: Sin re-notificación / re-arme**

Repetir PATCH `baby:true` → silencio. PATCH `baby:false` y luego `true`
→ re-notifica.

- [ ] **Step 5: "Agarra mi mano"**

Con `hr > 0`, mantener el dedo sobre el bebé → lub-dub mientras dura el
toque; para al soltar. Con `hr = 0` → no suena.

- [ ] **Step 6: Toggle Sonido**

Ajustes → Sonido OFF corta el sonido en curso; reboot → sin jingle; el
switch sigue OFF. Volver a ON → todo suena de nuevo.

- [ ] **Step 7: Volumen del latido**

Si el lub-dub (330/262 Hz) suena demasiado flojo en el buzzer físico,
subir una octava: `HB_LUB_HZ 659`, `HB_DUB_HZ 523` en `sound.cpp`, y
recompilar/flashear.
