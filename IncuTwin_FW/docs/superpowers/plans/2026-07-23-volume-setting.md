# Nivel de volumen en ajustes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sustituir el switch on/off de sonido de la página de ajustes por
cuatro niveles de volumen (Apagado/Bajo/Medio/Alto) aplicados vía duty
LEDC y persistidos en NVS con migración desde la clave vieja.

**Architecture:** El módulo `src/sound/` gana una API de volumen 0..3 que
modula el duty del canal LEDC 2 tras cada `ledcWriteTone()`; la UI de
ajustes reemplaza el switch por una fila de 4 botones estilo idioma. La
página de ajustes se compacta verticalmente para que la fila quepa sin
scroll junto al footer.

**Tech Stack:** PlatformIO (`pio run -e crowpanel_advance_28`),
Arduino-ESP32 (LEDC, Preferences/NVS), LVGL 8, C++17.

**Spec:** `docs/superpowers/specs/2026-07-21-volume-setting-design.md`

## Global Constraints

- Pantalla 240×320 portrait; textos cortos (UI para niños/mayores).
- Todo el sonido corre en el hilo de UI (lv_timer); no crear tareas.
- NVS namespace `"incutwin"`; clave nueva `"vol"` (uint8, default 3);
  migración desde `"sound"` (bool) si `"vol"` no existe.
- Duty por nivel (LEDC 10 bits): `{0, 8, 60, 512}` (calibrar en HW en la
  verificación final).
- Commits Conventional Commits con scope `hmi`; autor único Pablo Sánchez
  Bergasa, sin `Co-Authored-By`.
- No hay entorno de test nativo para estos módulos (LEDC/LVGL): el ciclo
  de cada tarea es compilar (`pio run -e crowpanel_advance_28`, esperar
  `SUCCESS`) + verificación manual final en hardware (Tarea 4).
- No tocar los ficheros con cambios sin commitear de otra feature
  (`src/net/firebase_stream.cpp`, `src/net/sim_server.cpp`,
  `src/ui/baby_widget.cpp`, `.claude/logs/loop.log`): en los commits,
  añadir SOLO los ficheros listados en cada tarea.

---

### Task 1: Cadenas i18n de los niveles de volumen

**Files:**
- Modify: `src/ui/i18n.h` (enum `str_id_t`)
- Modify: `src/ui/i18n.cpp` (tabla `STRINGS`)

**Interfaces:**
- Produces: `STR_VOL_OFF`, `STR_VOL_LOW`, `STR_VOL_MID`, `STR_VOL_HIGH`
  (ids consecutivos insertados justo después de `STR_SOUND`; la Tarea 3
  los consume vía `tr()`).

El enum y la tabla están acoplados por posición: los cuatro ids nuevos se
insertan en el MISMO punto en ambos ficheros (tras `STR_SOUND`).

- [ ] **Step 1: Añadir los ids al enum**

En `src/ui/i18n.h`, localizar:

```c
    STR_SOUND,
    /* status bar + hand button */
```

y dejarlo así:

```c
    STR_SOUND,
    STR_VOL_OFF,
    STR_VOL_LOW,
    STR_VOL_MID,
    STR_VOL_HIGH,
    /* status bar + hand button */
```

- [ ] **Step 2: Añadir las traducciones en la tabla**

En `src/ui/i18n.cpp`, localizar:

```c
    /* STR_SOUND         */ {"Sonido", "Sound"},
    /* STR_ST_NO_WIFI    */ {"Sin conexión WiFi", "No WiFi connection"},
```

y dejarlo así:

```c
    /* STR_SOUND         */ {"Sonido", "Sound"},
    /* STR_VOL_OFF       */ {"Apagado", "Off"},
    /* STR_VOL_LOW       */ {"Bajo", "Low"},
    /* STR_VOL_MID       */ {"Medio", "Mid"},
    /* STR_VOL_HIGH      */ {"Alto", "High"},
    /* STR_ST_NO_WIFI    */ {"Sin conexión WiFi", "No WiFi connection"},
```

- [ ] **Step 3: Compilar**

Run: `pio run -e crowpanel_advance_28`
Expected: `SUCCESS` (nada usa aún los ids nuevos).

- [ ] **Step 4: Commit**

```bash
git add src/ui/i18n.h src/ui/i18n.cpp
git commit -m "feat(hmi): cadenas i18n para los niveles de volumen"
```

---

### Task 2: API de volumen en el módulo de sonido

**Files:**
- Modify: `src/sound/sound.h`
- Modify: `src/sound/sound.cpp`

**Interfaces:**
- Produces (para la Tarea 3):
  - `void sound_set_volume(uint8_t level);` — 0..3, satura >3 a 3,
    persiste en NVS, nivel 0 corta todo lo que suene.
  - `uint8_t sound_get_volume(void);` — nivel actual 0..3.
  - `void sound_play_test(void);` — pitido corto (C6, 120 ms) al volumen
    actual.
- Mantiene TEMPORALMENTE `sound_set_enabled`/`sound_is_enabled` como
  wrappers para que `ui_main.cpp` siga compilando; la Tarea 3 los borra.

- [ ] **Step 1: Actualizar el header**

En `src/sound/sound.h`, actualizar la última línea del comentario de
cabecera:

```c
 *   - on/off toggle persisted in NVS ("incutwin"/"sound", default on)
```

pasa a ser:

```c
 *   - 4-level volume (off/low/mid/high) via LEDC duty, persisted in
 *     NVS ("incutwin"/"vol", default high; migrates the old on/off
 *     "sound" key)
```

Y sustituir las declaraciones:

```c
void sound_init(void);   /* LEDC channel + NVS + lv_timer */
void sound_set_enabled(bool on);
bool sound_is_enabled(void);
```

por:

```c
void sound_init(void);   /* LEDC channel + NVS + lv_timer */
void sound_set_volume(uint8_t level); /* 0 off .. 3 high; persists NVS */
uint8_t sound_get_volume(void);
void sound_play_test(void); /* short beep to preview the active level  */

/* deprecated on/off wrappers — removed when the UI switches to levels */
void sound_set_enabled(bool on);
bool sound_is_enabled(void);
```

- [ ] **Step 2: Estado de volumen y duty en tone_out**

En `src/sound/sound.cpp`, sustituir:

```c
static bool s_enabled = true;
```

por:

```c
/* volume: 0 off, 1 low, 2 mid, 3 high. Duty at the 10-bit LEDC
 * resolution ledcWriteTone() configures; loudness is roughly
 * logarithmic in duty, values to be calibrated on hardware. */
static const uint16_t VOLUME_DUTY[] = {0, 8, 60, 512};
static uint8_t s_volume = 3;
```

Añadir junto a las melodías (tras `PARENTS_MELODY`):

```c
/* settings-page preview beep (C6) */
static const melody_note_t TEST_MELODY[] = {{1047, 120}};
```

Sustituir `tone_out`:

```c
static void tone_out(uint16_t freq) { ledcWriteTone(BUZZER_LEDC_CH, freq); }
```

por:

```c
static void tone_out(uint16_t freq) {
    if (freq == 0 || s_volume == 0) {
        ledcWriteTone(BUZZER_LEDC_CH, 0);
        return;
    }
    ledcWriteTone(BUZZER_LEDC_CH, freq); /* leaves ~50 % duty */
    ledcWrite(BUZZER_LEDC_CH, VOLUME_DUTY[s_volume]);
}
```

- [ ] **Step 3: Sustituir s_enabled por s_volume en los guards**

En `hb_active()`:

```c
    if (!s_enabled || s_bpm == 0) return false;
```

pasa a:

```c
    if (s_volume == 0 || s_bpm == 0) return false;
```

En `seq_start()`:

```c
    if (!s_enabled) return;
```

pasa a:

```c
    if (s_volume == 0) return;
```

- [ ] **Step 4: Carga NVS con migración en sound_init**

Sustituir el bloque de Preferences de `sound_init()`:

```c
    Preferences p;
    p.begin("incutwin", true);
    s_enabled = p.getBool("sound", true);
    p.end();
```

por:

```c
    Preferences p;
    p.begin("incutwin", false);
    uint8_t vol = p.getUChar("vol", 0xFF);
    if (vol == 0xFF) { /* first boot on this fw: migrate the old key */
        vol = p.getBool("sound", true) ? 3 : 0;
        p.putUChar("vol", vol);
    }
    p.end();
    s_volume = vol > 3 ? 3 : vol;
```

- [ ] **Step 5: API nueva + wrappers**

Sustituir `sound_set_enabled` y `sound_is_enabled` completos:

```c
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
        s_hb_idle = true;
    }
}

bool sound_is_enabled(void) { return s_enabled; }
```

por:

```c
void sound_set_volume(uint8_t level) {
    if (level > 3) level = 3;
    if (level == s_volume) return;
    s_volume = level;
    Preferences p;
    p.begin("incutwin", false);
    p.putUChar("vol", level);
    p.end();
    if (level == 0) { /* cut anything currently playing */
        seq_stop();
        s_hb_until = 0;
        s_hb_phase = 0;
        s_hb_idle = true;
    }
}

uint8_t sound_get_volume(void) { return s_volume; }

void sound_play_test(void) {
    seq_start(TEST_MELODY, ARRAY_LEN(TEST_MELODY));
}

/* deprecated wrappers — removed when the UI switches to levels */
void sound_set_enabled(bool on) { sound_set_volume(on ? 3 : 0); }

bool sound_is_enabled(void) { return s_volume > 0; }
```

- [ ] **Step 6: Compilar**

Run: `pio run -e crowpanel_advance_28`
Expected: `SUCCESS`. Si aparece `s_enabled` sin declarar, queda algún uso
sin migrar en el paso 3 o 5.

- [ ] **Step 7: Commit**

```bash
git add src/sound/sound.h src/sound/sound.cpp
git commit -m "feat(hmi): niveles de volumen en el modulo de sonido con migracion NVS"
```

---

### Task 3: Selector de 4 niveles en la página de ajustes

**Files:**
- Modify: `src/ui/ui_main.cpp`
- Modify: `src/sound/sound.h` (borrar wrappers deprecated)
- Modify: `src/sound/sound.cpp` (borrar wrappers deprecated)

**Interfaces:**
- Consumes: `sound_set_volume(uint8_t)`, `sound_get_volume()`,
  `sound_play_test()` (Tarea 2); `STR_VOL_OFF/LOW/MID/HIGH` (Tarea 1).
- Produces: nada para tareas posteriores.

La página se compacta para que la fila de botones quepa sobre el footer
(el footer arranca en y≈270; la fila termina en y=266). Nuevo layout
vertical: idioma 56/76 (botones h40), tono de piel 124/144, hint 182,
sonido 214/234 (botones h32).

- [ ] **Step 1: Sustituir los estáticos del switch**

En `src/ui/ui_main.cpp`, sustituir:

```c
static lv_obj_t *s_lbl_sound, *s_sw_sound;
```

por:

```c
static lv_obj_t *s_lbl_sound, *s_btn_vol[4];
static const str_id_t VOL_STR[4] = {STR_VOL_OFF, STR_VOL_LOW, STR_VOL_MID,
                                    STR_VOL_HIGH};
```

- [ ] **Step 2: Sustituir el callback del switch por los de botones**

Sustituir:

```c
static void sound_switch_changed(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    sound_set_enabled(lv_obj_has_state(sw, LV_STATE_CHECKED));
}
```

por:

```c
static void vol_btns_refresh(void) {
    for (int i = 0; i < 4; i++) {
        bool sel = sound_get_volume() == (uint8_t)i;
        lv_obj_set_style_bg_color(s_btn_vol[i], sel ? COL_NAVY : COL_CARD, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(s_btn_vol[i], 0),
                                    sel ? lv_color_white() : COL_NAVY, 0);
    }
}

static void vol_clicked(lv_event_t *e) {
    uint8_t level = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (level == sound_get_volume()) return;
    sound_set_volume(level);
    vol_btns_refresh();
    if (level > 0) sound_play_test(); /* preview the new loudness */
}
```

- [ ] **Step 3: Compactar el layout de build_settings**

Cambios de coordenadas (mismo orden en que aparecen en `build_settings`):

| Objeto | Antes | Después |
|---|---|---|
| `s_lbl_lang` pos | `12, 62` | `12, 56` |
| `s_btn_es` size | `104, 44` | `104, 40` |
| `s_btn_es` pos | `12, 84` | `12, 76` |
| `s_btn_en` size | `104, 44` | `104, 40` |
| `s_btn_en` pos | `124, 84` | `124, 76` |
| `s_lbl_tone` pos | `12, 144` | `12, 124` |
| swatches pos | `12 + i * 37, 166` | `12 + i * 37, 144` |
| `s_lbl_hint` pos | `12, 208` | `12, 182` |
| `s_lbl_sound` pos | `12, 248` | `12, 214` |

- [ ] **Step 4: Sustituir la creación del switch por la fila de botones**

Sustituir el bloque:

```c
    s_sw_sound = lv_switch_create(scr_settings);
    lv_obj_set_size(s_sw_sound, 56, 30);
    lv_obj_set_pos(s_sw_sound, 172, 242);
    lv_obj_set_style_bg_color(s_sw_sound, COL_NAVY, LV_PART_INDICATOR |
                                                        LV_STATE_CHECKED);
    if (sound_is_enabled()) lv_obj_add_state(s_sw_sound, LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_sw_sound, sound_switch_changed,
                        LV_EVENT_VALUE_CHANGED, nullptr);
```

por:

```c
    static const int16_t VOL_X[4] = {12, 74, 124, 182};
    static const int16_t VOL_W[4] = {56, 44, 52, 44};
    for (int i = 0; i < 4; i++) {
        s_btn_vol[i] = lv_btn_create(scr_settings);
        lv_obj_set_size(s_btn_vol[i], VOL_W[i], 32);
        lv_obj_set_pos(s_btn_vol[i], VOL_X[i], 234);
        lv_obj_set_style_radius(s_btn_vol[i], 10, 0);
        lv_obj_set_style_border_width(s_btn_vol[i], 2, 0);
        lv_obj_set_style_border_color(s_btn_vol[i], COL_NAVY, 0);
        lv_obj_set_style_shadow_width(s_btn_vol[i], 0, 0);
        lv_obj_add_event_cb(s_btn_vol[i], vol_clicked, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        lv_obj_t *vl = lv_label_create(s_btn_vol[i]);
        lv_label_set_text(vl, tr(VOL_STR[i]));
        lv_obj_set_style_text_font(vl, &lv_font_es_12, 0);
        lv_obj_center(vl);
    }
```

Y al final de `build_settings`, donde dice:

```c
    lang_btns_refresh();
    swatch_refresh();
```

dejar:

```c
    lang_btns_refresh();
    swatch_refresh();
    vol_btns_refresh();
```

- [ ] **Step 5: Refrescar etiquetas al cambiar idioma**

En `update_texts()`, tras la línea:

```c
    lv_label_set_text(s_lbl_sound, tr(STR_SOUND));
```

añadir:

```c
    for (int i = 0; i < 4; i++)
        lv_label_set_text(lv_obj_get_child(s_btn_vol[i], 0), tr(VOL_STR[i]));
```

- [ ] **Step 6: Borrar los wrappers deprecated**

En `src/sound/sound.h` eliminar:

```c
/* deprecated on/off wrappers — removed when the UI switches to levels */
void sound_set_enabled(bool on);
bool sound_is_enabled(void);
```

En `src/sound/sound.cpp` eliminar:

```c
/* deprecated wrappers — removed when the UI switches to levels */
void sound_set_enabled(bool on) { sound_set_volume(on ? 3 : 0); }

bool sound_is_enabled(void) { return s_volume > 0; }
```

- [ ] **Step 7: Compilar ambos entornos**

Run: `pio run -e crowpanel_advance_28 && pio run -e crowpanel_advance_28_sim`
Expected: `SUCCESS` en ambos. Si aparece `sound_is_enabled` sin declarar,
queda alguna llamada vieja sin sustituir.

- [ ] **Step 8: Commit**

```bash
git add src/ui/ui_main.cpp src/sound/sound.h src/sound/sound.cpp
git commit -m "feat(hmi): selector de volumen de cuatro niveles en ajustes"
```

---

### Task 4: Documentación y verificación manual en hardware

**Files:**
- Modify: `README.md:24-27` (bullet del buzzer)

**Interfaces:**
- Consumes: todo lo anterior.

- [ ] **Step 1: Actualizar el README**

Sustituir:

```markdown
- **Notificaciones retro por buzzer**: jingle 8-bit al arrancar, fanfarria
  cuando la incubadora detecta al bebé (`baby`) con 10 s de latido
  audible, y "lub-dub" al bpm real mientras tocas al bebé. Silenciable
  desde Ajustes (persistente).
```

por:

```markdown
- **Notificaciones retro por buzzer**: jingle 8-bit al arrancar, fanfarria
  cuando la incubadora detecta al bebé (`baby`) con 10 s de latido
  audible, y "lub-dub" al bpm real mientras tocas al bebé. Volumen de 4
  niveles (Apagado/Bajo/Medio/Alto) desde Ajustes (persistente).
```

- [ ] **Step 2: Flashear**

Run: `pio run -e crowpanel_advance_28 -t upload`
Expected: `SUCCESS` (panel conectado por USB).

- [ ] **Step 3: Verificación manual (requiere humano con el panel)**

PAUSA: pedir al usuario que verifique en el hardware y confirme cada
punto. Si Bajo/Medio no se distinguen, ajustar `VOLUME_DUTY` en
`src/sound/sound.cpp` (subir/bajar 8 y 60) y repetir.

1. Los cuatro botones aparecen bajo "Sonido" sin pisar el footer, y el
   nivel activo se resalta en navy.
2. Tocar Bajo/Medio/Alto suena un pitido con sonoridad claramente
   distinta en cada nivel; tocar Apagado no pita.
3. Con Apagado, la fanfarria de bebé y el latido no suenan.
4. Reiniciar el panel: el nivel elegido persiste.
5. Migración: en un panel que tuviera el switch en off (clave NVS
   `sound=false` sin `vol`), arranca en Apagado.
6. Cambiar de idioma ES↔EN refresca Apagado/Bajo/Medio/Alto ↔
   Off/Low/Mid/High.

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "docs(hmi): documentar los niveles de volumen del buzzer"
```
