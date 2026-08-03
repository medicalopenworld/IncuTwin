# Estado "Baby with parents" — plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Al caer `show_baby` (bebé fuera o conexión perdida), mostrar la
imagen full-screen `Baby_with_parents` con melodía festiva durante 15 min,
cancelable si el bebé vuelve; después, volver al estado real.

**Architecture:** Override en la capa UI: `ui_apply_state()` detecta el
flanco de bajada de `show_baby`, abre una ventana de 15 min y fuerza una
tercera vista en `home_set_view()`. Sin cambios en `twin_state_t` ni en la
tarea de red. Spec: `docs/superpowers/specs/2026-07-21-baby-with-parents-design.md`.

**Tech Stack:** LVGL 8.3 (C++17 Arduino/ESP32-S3), buzzer LEDC 8-bit,
Pillow para generar assets (RGB565).

## Global Constraints

- No hay entorno de test nativo para UI/sonido: cada task se verifica con
  `pio run -e crowpanel_advance_28_sim` (compila) y la verificación
  funcional final es manual en hardware con el build `_sim`.
- Conventional Commits, scope `hmi`, autor único Pablo Sánchez Bergasa
  (sin `Co-Authored-By`). Rama de trabajo: `feat/sim-mode`.
- OJO: el árbol tiene cambios sin commitear de la iteración sim-mode
  (`ui_main.cpp`, `sound.*`, etc.). En cada commit añade SOLO los ficheros
  del task (`git add <paths>`), nunca `git add -A`.
- Strings de UI cortos (pantalla de 240 px; lectores niños/mayores).
- LVGL 8 + imágenes indexadas: nada de `lv_img_set_zoom/angle` (no aplica
  aquí: la imagen nueva es TRUE_COLOR estática).

---

### Task 1: Asset `img_baby_parents`

**Files:**
- Modify: `tools/gen_extra.py` (nueva función + emisión + externs)
- Modify: `src/assets/assets.h` (extern nuevo)
- Regenerated: `src/assets/img_extra.c` (lo reescribe el script)
- Add: `Images/Baby_with_parents.png` (ya está en el árbol, sin trackear)

**Interfaces:**
- Consumes: `Images/Baby_with_parents.png` (896×1199 RGBA, fondo blanco).
- Produces: `const lv_img_dsc_t img_baby_parents` (240×320, RGB565
  TRUE_COLOR sin alfa), declarada en `src/assets/assets.h`.

- [ ] **Step 1: Añadir la función a `tools/gen_extra.py`**

Tras `make_incunest_empty()` (línea ~124), añadir:

```python
def make_baby_parents():
    """Images/Baby_with_parents.png upright on the portrait screen, same
    treatment as the empty incubator: crop white margins, pad to 240x320."""
    im = Image.open(os.path.join(IMAGES, "Baby_with_parents.png")).convert("RGB")
    bg = Image.new("RGB", im.size, (255, 255, 255))
    diff = ImageChops.difference(im, bg).convert("L")
    bbox = diff.point(lambda v: 255 if v > 16 else 0).getbbox()
    im = im.crop(bbox)
    return ImageOps.pad(im, (SCREEN_W, SCREEN_H), Image.LANCZOS,
                        color=(255, 255, 255))
```

En `EXTERNS` (línea ~149) añadir al final, antes de la comilla de cierre:

```
extern const lv_img_dsc_t img_baby_parents;
```

En `main()`: generar, previsualizar y emitir junto a `empty`:

```python
    parents = make_baby_parents()          # tras `empty = make_incunest_empty()`
    parents.save(os.path.join(PREVIEW, "baby_parents.png"))   # junto al resto de saves
    emit_true_color("img_baby_parents", parents, f)           # tras emit_true_color de img_incunest_empty
```

- [ ] **Step 2: Añadir el extern a `src/assets/assets.h`**

El bloque de append del script no se ejecuta (está keyed en
`img_incunest_empty`, que ya existe), así que a mano, tras
`extern const lv_img_dsc_t img_incunest_empty;`:

```c
extern const lv_img_dsc_t img_baby_parents;
```

- [ ] **Step 3: Regenerar assets**

Run: `python tools/gen_extra.py` (desde la raíz del repo)
Expected: imprime `img_extra.c written`; `git diff --stat src/assets/img_extra.c`
muestra un crecimiento de ~9500 líneas; existe `tools/preview/baby_parents.png`
(ábrela: bebé con padres centrado, bandas blancas arriba/abajo).

- [ ] **Step 4: Verificar que compila**

Run: `pio run -e crowpanel_advance_28_sim`
Expected: `SUCCESS`. Flash sube ~150 KB respecto al build anterior (16 MB
de flash: sobra margen).

- [ ] **Step 5: Commit**

```bash
git add Images/Baby_with_parents.png tools/gen_extra.py src/assets/assets.h src/assets/img_extra.c
git commit -m "feat(hmi): asset full-screen del bebe con sus padres"
```

---

### Task 2: Melodía festiva `sound_play_parents()`

**Files:**
- Modify: `src/sound/sound.h` (declaración + doc de cabecera)
- Modify: `src/sound/sound.cpp` (melodía + función)

**Interfaces:**
- Consumes: infraestructura existente `seq_start()` / `melody_note_t`.
- Produces: `void sound_play_parents(void);` — no bloqueante, respeta el
  toggle de sonido (gating dentro de `seq_start`), la llama la UI en el
  flanco de salida del bebé.

- [ ] **Step 1: Declarar la API en `src/sound/sound.h`**

Tras `void sound_play_boot(void);`:

```c
void sound_play_parents(void); /* festive jingle: baby out with parents */
```

Y en el comentario de cabecera del fichero, añadir a la lista:

```
 *   - festive "baby with parents" jingle when the baby leaves the
 *     incubator (triggered by the UI on the show_baby falling edge)
```

- [ ] **Step 2: Implementar en `src/sound/sound.cpp`**

Junto a `BABY_MELODY` (línea ~21), añadir la fanfarria: tres arpegios
ascendentes de Do mayor encadenados y remate sostenido, ~2 s:

```c
/* three rising C-major arpeggios + held top note: "level clear" feel */
static const melody_note_t PARENTS_MELODY[] = {
    {523, 80},  {659, 80},  {784, 80},  {1047, 80},   /* C5 E5 G5 C6 */
    {659, 80},  {784, 80},  {1047, 80}, {1319, 80},   /* E5 G5 C6 E6 */
    {784, 80},  {1047, 80}, {1319, 80}, {1568, 200},  /* G5 C6 E6 G6 */
    {0, 60},    {1568, 80}, {0, 40},    {2093, 400},  /* ta-ta... C7! */
};
```

Tras `sound_play_boot()` (línea ~160):

```c
void sound_play_parents(void) {
    seq_start(PARENTS_MELODY, ARRAY_LEN(PARENTS_MELODY));
}
```

- [ ] **Step 3: Verificar que compila**

Run: `pio run -e crowpanel_advance_28_sim`
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/sound/sound.h src/sound/sound.cpp
git commit -m "feat(hmi): fanfarria festiva de bebe con sus padres"
```

---

### Task 3: Ventana de 15 min y vista "con sus papás" en la UI

**Files:**
- Modify: `include/config.h` (constante de duración)
- Modify: `src/ui/i18n.h`, `src/ui/i18n.cpp` (string de barra de estado)
- Modify: `src/ui/ui_main.cpp` (imagen, tercera vista, flancos, barra)

**Interfaces:**
- Consumes: `img_baby_parents` (Task 1), `sound_play_parents()` (Task 2).
- Produces: comportamiento final; sin API nueva hacia fuera.

- [ ] **Step 1: Constante en `include/config.h`**

En la sección `/* ---- Comportamiento ----` tras `DATA_STALE_S`:

```c
#define PARENTS_MODE_MS (15UL * 60UL * 1000UL) /* ventana "con sus papás" */
```

- [ ] **Step 2: String nuevo en i18n**

`src/ui/i18n.h` — en el bloque `/* status bar + hand button */`, tras
`STR_ST_BABY_AWAKE,`:

```c
    STR_ST_PARENTS,
```

`src/ui/i18n.cpp` — en `STRINGS`, tras la fila `STR_ST_BABY_AWAKE`
(el orden DEBE coincidir con el enum):

```c
    /* STR_ST_PARENTS    */ {"Con sus papás", "With parents"},
```

- [ ] **Step 3: Imagen full-screen en `build_home()`**

`src/ui/ui_main.cpp` — declarar junto a `s_empty_img` (línea ~41):

```c
static lv_obj_t *s_empty_img, *s_parents_img, *s_wifi_img;
```

En `build_home()`, justo después del bloque de `s_empty_img` (línea ~237):

```c
    /* full-screen "baby with parents", shown for a while after the baby
     * leaves the incubator (see PARENTS_MODE_MS) */
    s_parents_img = lv_img_create(scr_home);
    lv_img_set_src(s_parents_img, &img_baby_parents);
    lv_obj_set_pos(s_parents_img, 0, 0);
    lv_obj_add_flag(s_parents_img, LV_OBJ_FLAG_HIDDEN);
```

- [ ] **Step 4: `home_set_view()` con tres vistas**

Sustituir la función actual (línea ~468) por:

```c
/* what fills the centre of the home screen */
typedef enum {
    VIEW_EMPTY = 0, /* empty incubator (offline / no baby) */
    VIEW_BABY,      /* animated baby + icons + hand button */
    VIEW_PARENTS,   /* baby out with parents               */
} home_view_t;

static void home_set_view(home_view_t view) {
    lv_obj_t *widgets[] = {baby_widget_obj(), s_icon_thermo, s_icon_photo,
                           s_icon_heart, s_btn_hand};
    for (auto *w : widgets) {
        if (view == VIEW_BABY)
            lv_obj_clear_flag(w, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(w, LV_OBJ_FLAG_HIDDEN);
    }
    if (view == VIEW_EMPTY)
        lv_obj_clear_flag(s_empty_img, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(s_empty_img, LV_OBJ_FLAG_HIDDEN);
    if (view == VIEW_PARENTS)
        lv_obj_clear_flag(s_parents_img, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(s_parents_img, LV_OBJ_FLAG_HIDDEN);
}
```

- [ ] **Step 5: Flancos y ventana en `ui_apply_state()`**

Tras el cálculo de `show_baby` (y su traza serie), añadir:

```c
    /* ventana "con sus papás": cualquier caída de show_baby la abre;
     * el bebé de vuelta la cancela. Resta con signo por el rollover. */
    static bool prev_baby = false;
    static uint32_t parents_until = 0; /* 0 = inactiva */
    if (!show_baby && prev_baby) {
        parents_until = millis() + PARENTS_MODE_MS;
        if (parents_until == 0) parents_until = 1;
        sound_play_parents();
    } else if (show_baby) {
        parents_until = 0;
    }
    prev_baby = show_baby;
    bool parents = parents_until != 0 &&
                   (int32_t)(millis() - parents_until) < 0;
```

Y cambiar la llamada existente:

```c
    home_set_view(parents ? VIEW_PARENTS
                          : show_baby ? VIEW_BABY : VIEW_EMPTY);
```

(El halo no necesita cambios: `!show_baby` ya lo apaga con
`HALO_OFFLINE`, y `parents` implica `!show_baby`.)

- [ ] **Step 6: Barra de estado con prioridad "con sus papás"**

Anteponer una rama al `if` de la barra (línea ~520):

```c
    if (parents)
        status_bar_set(STR_ST_PARENTS, COL_OK);
    else if (!st.wifi_connected)
        status_bar_set(STR_ST_NO_WIFI, COL_RED);
```

(el resto de la cadena `else if` queda igual).

- [ ] **Step 7: Verificar que compila**

Run: `pio run -e crowpanel_advance_28_sim`
Expected: `SUCCESS`. También `pio run -e crowpanel_advance_28` (el build
de producción no usa nada del simulador).

- [ ] **Step 8: Commit**

```bash
git add include/config.h src/ui/i18n.h src/ui/i18n.cpp src/ui/ui_main.cpp
git commit -m "feat(hmi): estado temporal de bebe con sus padres al salir de la incubadora"
```

---

### Task 4: Verificación manual en hardware (build `_sim`)

**Files:**
- Ninguno (verificación); si algo falla, arreglar y commitear como `fix(hmi)`.

**Interfaces:**
- Consumes: web de control del simulador (`GET /` en la IP del panel) y
  `POST /state` con JSON.

- [ ] **Step 1: Flashear el build de simulación**

Run: `pio run -e crowpanel_advance_28_sim -t upload && pio device monitor`
Expected: arranca, imprime la IP por serie al conectar al WiFi.

- [ ] **Step 2: Bebé dentro → fuera**

En la web del simulador aplicar un escenario con bebé (o
`POST /state {"baby":true}` sobre un escenario online) y después
`POST /state {"baby":false}`.
Expected: imagen full-screen del bebé con sus padres, fanfarria festiva
una sola vez, barra "Con sus papás" en verde, halo e iconos ocultos.

- [ ] **Step 3: Cancelación por retorno**

`POST /state {"baby":true}` antes de que expire la ventana.
Expected: vuelve el bebé animado al instante; suena la fanfarria corta de
"bebé detectado" (comportamiento preexistente), no la de papás.

- [ ] **Step 4: Expiración de la ventana**

Para no esperar 15 min: rebajar temporalmente `PARENTS_MODE_MS` a
`(60UL * 1000UL)`, reflashear, repetir el paso 2 y esperar 1 min.
Expected: pasa solo a "IncuNest sin bebé" (incubadora vacía). Restaurar
la constante y reflashear al terminar.

- [ ] **Step 5: Caída de conexión con bebé dentro**

Con bebé dentro, aplicar el escenario offline/unlinked del simulador.
Expected: también entra a "con sus papás" (decisión del disparador de la
spec); al expirar, vista offline normal.

- [ ] **Step 6: Sonido apagado**

Desactivar el sonido en ajustes y repetir el paso 2.
Expected: imagen y barra cambian igual; no suena nada.

- [ ] **Step 7: Anotar el resultado**

Añadir al final de la spec una sección `## Resultado de verificación`
con fecha y el checklist 1-6 marcado, y commitear:

```bash
git add docs/superpowers/specs/2026-07-21-baby-with-parents-design.md
git commit -m "docs(hmi): verificacion manual del estado Baby with parents"
```
