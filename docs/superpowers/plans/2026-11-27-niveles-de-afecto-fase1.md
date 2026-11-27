# Niveles de afecto, fase 1 (pantalla) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Los niveles pasan a llamarse Manita, Caricia, Cosquillitas, Abrazo y Achuchón (0 / 10 / 30 / 75 / 150), y los dos primeros que abren algo (Caricia y Cosquillitas) funcionan en la pantalla.

**Architecture:** La lógica nueva es C puro y se prueba en `test_apps`: los umbrales y el «aviso pendiente» viven en `rewards_logic`, y el reconocimiento del dedo en un componente nuevo `affection`. La pantalla (`ui_home`, `baby_widget`, `ui_notice`, `ui_hearts`) solo conecta: pasa puntos del dedo al reconocedor y pinta la reacción. No toca el broker ni la IncuNest.

**Tech Stack:** ESP-IDF 6.0.1, C, LVGL 9, Unity on-target (`test_apps`), Python + Pillow (`tools/gen_assets.py`).

**Spec:** `docs/superpowers/specs/2026-11-27-niveles-de-afecto-design.md`. Este plan cubre solo la fase 1. Abrazo y Achuchón (acelerómetro y motor) van en otro plan cuando haya una placa montada.

## Global Constraints

- Rama `feat/afecto`, en el worktree `C:\Users\Pablo\Documents\IncuTwin-afecto` (sacada de `feat/acompanar` en 70e3132, firmware 2.0.3). El firmware está en `IncuTwin_FW/`; las rutas de este plan son relativas a esa carpeta salvo que empiecen por `docs/`.
- Entorno de compilación: `. C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1` (EIM), no `export.ps1`.
- La placa de pruebas está en COM12 y la comparte otra sesión: avisar a Pablo antes de flashear.
- Niveles: Manita 0, Caricia 10, Cosquillitas 30, Abrazo 75, Achuchón 150. ES/EN: Manita/Little hand, Caricia/Caress, Cosquillitas/Tickles, Abrazo/Hug, Achuchón/Bear hug.
- No subir `RW_VERSION` ni cambiar `sizeof(rw_state_t)`: lo nuevo sale de `reserved[]`. Nadie pierde corazones, sellos ni compras.
- Las interacciones nuevas no dan corazones, no cuentan para la racha ni el súper corazón, y no mandan nada a la IncuNest.
- Nunca rojo, alarma ni texto de culpa. Sin sonido en caricia y cosquillitas.
- La «risa» es suave: nada de carcajada.
- El dibujo de reposo (`sleep`) no se toca.
- En esta fase, Abrazo y Achuchón existen como nombres de nivel pero no anuncian ninguna interacción.
- Commits en español con el estilo del repo (`feat(hmi): …`, `feat(rewards): …`), uno por tarea. No hacer push.

## Review Focus

1. **Dedo quieto con ruido del táctil.** Un dedo apoyado 60 s tiembla unos píxeles: no debe contar como caricia; sigue saliendo la burbuja «Pulsa "Coge mi mano"…». → test en la tarea 2.
2. **Guardado antiguo con corazones.** Una IncuTwin con 10 o más ganados recibe el aviso de lo abierto una sola vez, también tras reiniciar, y la demo ni lo dispara ni lo gasta. → tests en las tareas 1 y 5.
3. **Caricia interrumpida.** El dedo sale del bebé, llega un `PRESS_LOST`, o se pulsa «Coge mi mano» a mitad: la cara vuelve al reposo que toque (dormido o despierto) y no se queda la sonrisa puesta. → comprobación en la tarea 4.
4. **Reacción encima de otra animación.** Caricia durante un bostezo, o cambio de sexo, tono o ropa durante la reacción: el fotograma final es el de reposo correcto del dibujo correcto. → comprobación en la tarea 3.
5. **Caricia en la demo frente a deslizar página.** En demo, pasar el dedo en horizontal sobre el bebé con nivel Caricia acaricia y no cambia de página; fuera del bebé sigue cambiando. → comprobación en la tarea 4.

---

### Task 1: Nombres, umbrales y aviso pendiente (`rewards_logic`)

**Files:**
- Modify: `components/rewards/include/rewards_logic.h`, `components/rewards/rewards_logic.c`
- Modify: `components/ui/ui_i18n.h`, `components/ui/ui_i18n.c`, `components/ui/ui_rewards.c`
- Modify: `openspec/changes/hmi-corazones/specs/rewards/spec.md` (requisito «Niveles»)
- Test: `test_apps/main/test_rewards.c`

**Interfaces:**
- Produces:
  - `typedef enum { RW_LEVEL_HAND = 0, RW_LEVEL_CARESS, RW_LEVEL_TICKLES, RW_LEVEL_HUG, RW_LEVEL_SQUEEZE, RW_LEVEL_COUNT } rw_level_t;` (sustituye a `RW_LEVEL_SPARK … RW_LEVEL_SUN`; cambiar todos los usos).
  - Campo `uint8_t level_seen;` en `rw_state_t`, tomado de `reserved` (que pasa a `reserved[18]`). Es el nivel más alto ya anunciado.
  - `bool rw_catchup_due(const rw_state_t *s);` — true si `rw_level(s->earned) > s->level_seen`.
  - `void rw_catchup_done(rw_state_t *s);` — pone `level_seen = rw_level(s->earned)`.
  - `STR_LEVEL_HAND, STR_LEVEL_CARESS, STR_LEVEL_TICKLES, STR_LEVEL_HUG, STR_LEVEL_SQUEEZE` (sustituyen a los `STR_LEVEL_*` actuales, en el mismo sitio del enum).

- [ ] **Step 1: Cambiar el test de niveles y añadir los del aviso pendiente**

```c
TEST_CASE("rewards: niveles de afecto por lo ganado; gastar no baja", "[rewards]") {
    TEST_ASSERT_EQUAL(RW_LEVEL_HAND, rw_level(9));
    TEST_ASSERT_EQUAL(RW_LEVEL_CARESS, rw_level(10));
    TEST_ASSERT_EQUAL(RW_LEVEL_TICKLES, rw_level(30));
    TEST_ASSERT_EQUAL(RW_LEVEL_HUG, rw_level(75));
    TEST_ASSERT_EQUAL(RW_LEVEL_SQUEEZE, rw_level(150));
    TEST_ASSERT_EQUAL_UINT32(10, rw_level_min(RW_LEVEL_CARESS));
    rw_state_t s; rw_init(&s);
    s.earned = 9; s.balance = 9; s.last_day = D0 - 1; s.streak = 1;
    rw_gesture_t g = G(D0, "TG", 7);
    TEST_ASSERT_TRUE(rw_on_gesture(&s, &g).level_up);
    uint16_t missing = 0;
    TEST_ASSERT_EQUAL(RW_UNLOCK_OK, rw_unlock(&s, RW_ITEM_BG_CREAM, NOV, &missing)); /* 10 */
    TEST_ASSERT_EQUAL(RW_LEVEL_CARESS, rw_level(s.earned));
}

TEST_CASE("rewards: quien ya tenia corazones recibe el aviso una vez", "[rewards]") {
    rw_state_t s; rw_init(&s);
    TEST_ASSERT_FALSE(rw_catchup_due(&s));          /* Manita: nada que anunciar */
    s.earned = 40;                                   /* guardado antiguo: level_seen = 0 */
    TEST_ASSERT_TRUE(rw_catchup_due(&s));
    rw_catchup_done(&s);
    TEST_ASSERT_FALSE(rw_catchup_due(&s));
    TEST_ASSERT_EQUAL_UINT8(RW_LEVEL_TICKLES, s.level_seen);
}

TEST_CASE("rewards: subir de nivel jugando no deja aviso pendiente", "[rewards]") {
    rw_state_t s; rw_init(&s);
    s.earned = 9; s.last_day = D0 - 1; s.streak = 1;
    rw_gesture_t g = G(D0, "TG", 7);
    TEST_ASSERT_TRUE(rw_on_gesture(&s, &g).level_up);
    TEST_ASSERT_FALSE(rw_catchup_due(&s));          /* ya lo anuncia el aviso normal */
}

TEST_CASE("rewards: el guardado no cambia de tamaño", "[rewards]") {
    TEST_ASSERT_EQUAL_UINT(offsetof(rw_state_t, passport) - 19, offsetof(rw_state_t, level_seen));
}
```

- [ ] **Step 2: Compilar los tests y ver que fallan**

Run: `idf.py -C test_apps build`
Expected: error de compilación (`RW_LEVEL_HAND` y `rw_catchup_due` no existen).

- [ ] **Step 3: Implementar en `rewards_logic.h/.c`**

`LEVEL_MIN = { 0, 10, 30, 75, 150 }`. `earn()` pone `level_seen` al nivel nuevo cuando marca `level_up`. `rw_migrate_v1()` deja `level_seen = 0` (para que el aviso pendiente salga). Añadir junto a los otros: `_Static_assert(sizeof(rw_state_t) == <el valor actual>, "el guardado v2 no puede cambiar");` con el número que dé `sizeof` antes del cambio.

- [ ] **Step 4: Textos y nombres**

En `ui_i18n.c`: `{ "Manita", "Little hand" }`, `{ "Caricia", "Caress" }`, `{ "Cosquillitas", "Tickles" }`, `{ "Abrazo", "Hug" }`, `{ "Achuchón", "Bear hug" }`. Actualizar la tabla de `ui_level_name()` en `ui_rewards.c`. Comprobar con `python tools/gen_fonts.py` (o como se regeneren las fuentes) que «ó» está en `lv_font_es_16` y `lv_font_es_12`; si no, regenerar.

- [ ] **Step 5: Actualizar la spec**

En `openspec/changes/hmi-corazones/specs/rewards/spec.md`, requisito «Niveles»: nombres y umbrales nuevos, escenario «Subir de nivel» con 9 → 10 (Manita a Caricia), «Gastar no baja» con Caricia, y el requisito «Demo aparte» con «nivel Caricia» (la demo empieza con 12 ganados).

- [ ] **Step 6: Pasar los tests en la placa**

Run: `idf.py -C test_apps -p COM12 flash` y `python tools/serial_capture.py --port COM12 --seconds 20 --reset`
Expected: todos los tests `[rewards]` en verde y 0 fallos en el total.

- [ ] **Step 7: Commit**

```bash
git add IncuTwin_FW/components/rewards IncuTwin_FW/components/ui/ui_i18n.* IncuTwin_FW/components/ui/ui_rewards.c IncuTwin_FW/test_apps/main/test_rewards.c IncuTwin_FW/openspec/changes/hmi-corazones/specs/rewards/spec.md
git commit -m "feat(rewards): niveles de afecto (Manita, Caricia, Cosquillitas, Abrazo, Achuchón)"
```

---

### Task 2: Reconocer caricia y cosquillas (`affection`, C puro)

**Files:**
- Create: `components/affection/CMakeLists.txt`, `components/affection/include/affection.h`, `components/affection/affection.c`
- Modify: `test_apps/main/CMakeLists.txt` (añadir `test_affection.c`, `${COMP}/affection/affection.c` y su `include`)
- Test: `test_apps/main/test_affection.c`

**Interfaces:**
- Consumes: `rw_level_t` de la tarea 1.
- Produces:

```c
typedef enum { AFF_NONE = 0, AFF_CARESS, AFF_TICKLES } aff_kind_t;

typedef struct {
    bool down;
    int16_t x, y;        /* ultimo punto contado */
    uint16_t path;       /* recorrido acumulado, px */
    int8_t dir;          /* sentido horizontal del ultimo tramo: -1, 0, +1 */
    uint8_t turns;       /* cambios de sentido dentro de la ventana */
    uint32_t turn_ms[4]; /* marcas de los ultimos cambios */
    aff_kind_t best;     /* lo mas alto reconocido en este toque */
} aff_stroke_t;

/* Lo que abre cada nivel: CARESS desde RW_LEVEL_CARESS, TICKLES desde RW_LEVEL_TICKLES */
bool aff_open(rw_level_t level, aff_kind_t kind);
void aff_down(aff_stroke_t *s, int16_t x, int16_t y, uint32_t now_ms);
/* Devuelve lo reconocido hasta ahora en este toque (nunca baja), limitado por el nivel */
aff_kind_t aff_move(aff_stroke_t *s, int16_t x, int16_t y, uint32_t now_ms, rw_level_t level);
void aff_up(aff_stroke_t *s);
```

Valores de partida (se ajustan con la placa en la tarea 4, y los tests se cambian con ellos):
`AFF_STEP_MIN_PX 6` (un movimiento menor no cuenta: ruido del táctil), `AFF_CARESS_PX 40` (recorrido para caricia), `AFF_TURN_MIN_PX 12` (tramo mínimo entre cambios de sentido), `AFF_TICKLE_TURNS 4` cambios de sentido en `AFF_TICKLE_WINDOW_MS 1500`.

- [ ] **Step 1: Escribir los tests**

```c
static aff_kind_t swipe(aff_stroke_t *s, int x0, int x1, int step, uint32_t *t, rw_level_t lv) {
    aff_kind_t k = AFF_NONE;
    for (int x = x0; step > 0 ? x <= x1 : x >= x1; x += step) { *t += 20; k = aff_move(s, x, 100, *t, lv); }
    return k;
}

TEST_CASE("affection: lo que abre cada nivel", "[affection]") {
    TEST_ASSERT_FALSE(aff_open(RW_LEVEL_HAND, AFF_CARESS));
    TEST_ASSERT_TRUE(aff_open(RW_LEVEL_CARESS, AFF_CARESS));
    TEST_ASSERT_FALSE(aff_open(RW_LEVEL_CARESS, AFF_TICKLES));
    TEST_ASSERT_TRUE(aff_open(RW_LEVEL_SQUEEZE, AFF_TICKLES));
}

TEST_CASE("affection: un dedo quieto que tiembla no es caricia", "[affection]") {
    aff_stroke_t s = { 0 }; uint32_t t = 0; aff_down(&s, 100, 100, t);
    for (int i = 0; i < 3000; i++) { t += 20; TEST_ASSERT_EQUAL(AFF_NONE, aff_move(&s, 100 + (i % 3) - 1, 100 + (i % 2), t, RW_LEVEL_SQUEEZE)); }
}

TEST_CASE("affection: recorrer 40 px es caricia", "[affection]") {
    aff_stroke_t s = { 0 }; uint32_t t = 0; aff_down(&s, 100, 100, t);
    TEST_ASSERT_EQUAL(AFF_NONE, swipe(&s, 100, 130, 6, &t, RW_LEVEL_CARESS));
    TEST_ASSERT_EQUAL(AFF_CARESS, swipe(&s, 136, 148, 6, &t, RW_LEVEL_CARESS));
}

TEST_CASE("affection: en Manita acariciar no hace nada", "[affection]") {
    aff_stroke_t s = { 0 }; uint32_t t = 0; aff_down(&s, 100, 100, t);
    TEST_ASSERT_EQUAL(AFF_NONE, swipe(&s, 100, 200, 6, &t, RW_LEVEL_HAND));
}

TEST_CASE("affection: ida y vuelta rapida es cosquillas, y en Caricia se queda en caricia", "[affection]") {
    for (int pass = 0; pass < 2; pass++) {
        rw_level_t lv = pass ? RW_LEVEL_TICKLES : RW_LEVEL_CARESS;
        aff_stroke_t s = { 0 }; uint32_t t = 0; aff_down(&s, 100, 100, t);
        aff_kind_t k = AFF_NONE;
        for (int i = 0; i < 3; i++) { k = swipe(&s, 100, 142, 6, &t, lv); k = swipe(&s, 142, 100, -6, &t, lv); }
        TEST_ASSERT_EQUAL(pass ? AFF_TICKLES : AFF_CARESS, k);
    }
}

TEST_CASE("affection: ida y vuelta lenta es caricia, no cosquillas", "[affection]") {
    aff_stroke_t s = { 0 }; uint32_t t = 0; aff_down(&s, 100, 100, t);
    aff_kind_t k = AFF_NONE;
    for (int i = 0; i < 3; i++) { k = swipe(&s, 100, 142, 1, &t, RW_LEVEL_TICKLES); k = swipe(&s, 142, 100, -1, &t, RW_LEVEL_TICKLES); }
    TEST_ASSERT_EQUAL(AFF_CARESS, k);   /* 42 px a 20 ms/px: cada tramo dura 840 ms */
}

TEST_CASE("affection: soltar y volver a tocar empieza de cero", "[affection]") {
    aff_stroke_t s = { 0 }; uint32_t t = 0; aff_down(&s, 100, 100, t);
    swipe(&s, 100, 160, 6, &t, RW_LEVEL_CARESS); aff_up(&s);
    aff_down(&s, 100, 100, t);
    TEST_ASSERT_EQUAL(AFF_NONE, aff_move(&s, 104, 100, t + 20, RW_LEVEL_CARESS));
}
```

- [ ] **Step 2: Compilar y ver que fallan**

Run: `idf.py -C test_apps build`
Expected: error de compilación (`affection.h` no existe).

- [ ] **Step 3: Implementar `affection.c`**

Sin LVGL ni FreeRTOS, como `hand_hold.c`. `aff_move` ignora movimientos de menos de `AFF_STEP_MIN_PX` respecto al último punto contado; un cambio de sentido se cuenta cuando el tramo en el sentido nuevo llega a `AFF_TURN_MIN_PX`; la ventana se mide con las marcas `turn_ms` y tolera el desbordamiento de 32 bits (restas sin signo).

- [ ] **Step 4: Pasar los tests en la placa**

Run: `idf.py -C test_apps -p COM12 flash` y `python tools/serial_capture.py --port COM12 --seconds 20 --reset`
Expected: los 7 tests `[affection]` en verde, 0 fallos en total.

- [ ] **Step 5: Commit**

```bash
git add IncuTwin_FW/components/affection IncuTwin_FW/test_apps/main
git commit -m "feat(affection): reconocer caricia y cosquillas a partir del dedo"
```

---

### Task 3: Caras nuevas y reacción del bebé (`gen_assets`, `baby_widget`)

**Files:**
- Modify: `tools/gen_assets.py` (`make_frames`), y lo que regenera: `components/assets/img_baby.c`, `components/assets/include/assets_baby.h`
- Modify: `components/ui/baby_widget.h`, `components/ui/baby_widget.c`
- Modify: `components/support_console/console_cmds.c` (comando para verlas)

**Interfaces:**
- Produces:
  - Fotogramas `img_baby_smile`, `img_baby_laugh`, `img_baby_girl_smile`, `img_baby_girl_laugh` (misma paleta común que los demás).
  - `typedef enum { BABY_REACT_CARESS = 0, BABY_REACT_TICKLES } baby_react_t;`
  - `void baby_react(baby_react_t r);` — pinta la reacción y vuelve sola al reposo. Llamarla durante una reacción la reinicia (o la sube de caricia a cosquillas); nunca la baja a mitad.
  - `bool baby_reacting(void);`
  - Duraciones: `BABY_CARESS_MS 2500`, `BABY_TICKLES_MS 2000` (tras las cosquillas, 1000 ms de cara de caricia).

Referencia del aspecto, aprobada por Pablo: `C:\Users\Pablo\Documents\IncuTwin\afecto-borrador\gemini_01_captura.png` (primera cara = caricia, segunda = cosquillitas). No usar `00_bocetos_afecto.png`.

- [ ] **Step 1: Dibujar los fotogramas en `make_frames`**

Con el mismo método que los bostezos (tapar la boca con piel y pintar encima), añadir tras `yawn2`:
- `smile`: ojos cerrados como en `sleep`, sonrisa más ancha y marcada que la de reposo, mofletes más grandes y más sonrosados.
- `laugh`: ojos cerrados en arco hacia arriba, boca abierta sonriendo (pequeña, con el interior oscuro y la lengua como en los bostezos), mismos mofletes.
Los mofletes se pintan con el color de rubor que ya reconoce `is_blush`, para que la paleta de tonos de piel los siga tratando como rubor. La niña sale de `girlify()` igual que los demás (pestañas incluidas en `smile`; en `laugh` las pestañas siguen el arco).

- [ ] **Step 2: Regenerar y mirar la previsualización**

Run: `python tools/gen_assets.py`
Expected: `tools/preview/frames.png` enseña `sleep, yawn1, yawn2, awake, smile, laugh` de niño y niña; `tools/preview/tones.png` y `outfits.png` siguen bien (los colores nuevos no rompen tonos, gorros ni mantas). Leer las tres imágenes y compararlas con la referencia.

- [ ] **Step 3: `baby_widget`: fotogramas y reacción**

`FR_SMILE` y `FR_LAUGH` al final de `frame_t` y de `S_SRC`. `baby_react()`:
- Corta un bostezo en marcha y aplaza el siguiente hasta que acabe la reacción.
- Caricia: `FR_SMILE` durante `BABY_CARESS_MS` y un corazón (`img_heart`, a 20 px de ancho) que sale junto a la cabeza, sube 30 px y se desvanece en 1200 ms.
- Cosquillas: `FR_LAUGH` durante `BABY_TICKLES_MS` con un vaivén del dibujo de ±3 px en horizontal (4 idas y vueltas), luego `FR_SMILE` 1000 ms.
- Oculta las «z Z z» mientras dura y las devuelve si toca.
- Al terminar: `show_frame(rest_frame())`.
- `baby_set_awake()` durante una reacción solo guarda el valor (como ya hace con el bostezo); `baby_set_girl()`, tono y ropa repintan el fotograma de la reacción en el dibujo nuevo.
- `baby_set_shown(false)` cancela la reacción.

- [ ] **Step 4: Comando de consola para verlas**

`bebe caricia` y `bebe cosquillas` llaman a `baby_react()` en la tarea de LVGL (con el mismo mecanismo que usan los demás comandos que tocan la pantalla). Ayuda: «prueba la reacción del bebé (no cuenta como uso)».

- [ ] **Step 5: Compilar, flashear y capturar**

Run: `idf.py build` y, avisando antes a Pablo, `idf.py -p COM12 flash`. Capturas con la consola `pantalla` + `python tools/screenshot.py`:
1. `bebe caricia` con niño y con niña.
2. `bebe cosquillas` con niño y con niña.
3. `bebe caricia` con un tono de piel oscuro y un gorro y una manta de la tienda puestos.
4. `bebe caricia` lanzado durante un bostezo, y captura 4 s después: el bebé está en reposo.
5. Cambio de sexo (comando de simulación que ya exista) durante `bebe cosquillas`: el dibujo cambia y termina en reposo.
Expected: las cinco se ven bien y ninguna deja la cara de reacción puesta.

- [ ] **Step 6: Enseñar las capturas a Pablo y esperar su visto bueno**

No seguir con la tarea 4 hasta que las apruebe; si pide cambios en las caras, volver al paso 1.

- [ ] **Step 7: Commit**

```bash
git add IncuTwin_FW/tools/gen_assets.py IncuTwin_FW/components/assets IncuTwin_FW/components/ui/baby_widget.* IncuTwin_FW/components/support_console/console_cmds.c
git commit -m "feat(hmi): caras de caricia y cosquillitas del bebe"
```

---

### Task 4: Acariciar al bebé en Inicio (`ui_home`) y contarlo

**Files:**
- Modify: `components/ui/ui_home.c` (`baby_touched`, `scr_gesture`), `components/ui/CMakeLists.txt` (dependencia `affection`)
- Modify: `components/usage_stats/include/usage_stats.h`, `components/usage_stats/usage_stats.c`
- Modify: `components/cloud_status/include/metrics_payload.h`, `components/cloud_status/metrics_payload.c`
- Test: `test_apps/main/test_metrics.c`

**Interfaces:**
- Consumes: `aff_down/aff_move/aff_up`, `aff_kind_t` (tarea 2); `baby_react`, `baby_reacting` (tarea 3); `rw_level(ui_rewards()->earned)`.
- Produces:
  - `void usage_affection(aff_kind_t kind);` — suma 1 al contador del día de ese tipo.
  - `usage_stats_t` gana `uint32_t caress_n, tickle_n;`
  - El JSON de métricas gana `"car_n"` y `"tic_n"` detrás de `"hand_n"`.

- [ ] **Step 1: Test de métricas**

En `test_metrics.c`, en el test que comprueba el JSON completo, dar `caress_n = 3`, `tickle_n = 1` y esperar `"car_n":3,"tic_n":1` tras `"hand_n":…`; y comprobar que el JSON más largo posible sigue cabiendo en `METRICS_PAYLOAD_MAX`.

- [ ] **Step 2: Ver que falla, implementar `usage_stats` y `metrics_payload`, ver que pasa**

Run: `idf.py -C test_apps -p COM12 flash` + `python tools/serial_capture.py --port COM12 --seconds 20 --reset`
Expected: primero falla el test de métricas; tras implementar, 0 fallos. Si el JSON no cabe, subir `METRICS_PAYLOAD_MAX` lo justo. Los contadores se guardan y se reinician igual que `hand_n`.

- [ ] **Step 3: Conectar el dedo en `baby_touched`**

Solo con `hh_state(&s_hh) == HH_IDLE` y el bebé a la vista:
- `LV_EVENT_PRESSED`: `aff_down()` con `lv_indev_get_point()`, y la burbuja `STR_INFO_PRESS_HAND` como hoy.
- `LV_EVENT_PRESSING`: `aff_move()` con el nivel actual (el de la demo si la demo está activa: `ui_rewards()` ya lo da). La primera vez que devuelve `AFF_CARESS` en este toque: `info_hide()`, `baby_react(BABY_REACT_CARESS)`, `usage_affection(AFF_CARESS)`. La primera vez que devuelve `AFF_TICKLES`: `baby_react(BABY_REACT_TICKLES)`, `usage_affection(AFF_TICKLES)`. En la demo no se cuenta el uso (como el resto del uso en demo).
- `LV_EVENT_RELEASED` / `LV_EVENT_PRESS_LOST`: `aff_up()`.
- Con el gesto «Coge mi mano» en marcha (cualquier estado que no sea `HH_IDLE`) el camino actual no cambia y no se llama a `aff_*`.
- `hand_btn_clicked` durante una reacción: la reacción se cancela (el gesto manda en los ojos).

- [ ] **Step 4: Que la caricia no cambie de página en la demo**

En `scr_gesture`: si el toque en curso empezó sobre el bebé y el nivel abre la caricia (`aff_open(level, AFF_CARESS)`), no llamar a `ui_pages_swipe`.

- [ ] **Step 5: Probar en la placa y ajustar umbrales**

Avisar a Pablo, flashear, y con `demo on` (nivel Caricia) y luego con el nivel subido por consola (tarea 5; hasta entonces, forzar `earned` con el depurador o dejar este punto para después de la tarea 5):
1. Dedo quieto 10 s sobre el bebé: sale la burbuja, no sonríe.
2. Pasar el dedo despacio: sonríe y sube el corazón; no cambia de página.
3. Deslizar fuera del bebé en demo: cambia de página.
4. Acariciar y salir con el dedo del bebé: la cara vuelve al reposo.
5. Acariciar y pulsar «Coge mi mano»: empieza el gesto normal con los ojos abiertos.
6. En nivel Manita (fuera de demo, `earned` < 10): acariciar no hace nada nuevo.
Si la caricia cuesta o salta sola, ajustar `AFF_*` en `affection.c` y los tests de la tarea 2 con ellos.
Expected: los seis casos se comportan así; `rewards` en la consola no cambia de saldo.

- [ ] **Step 6: Commit**

```bash
git add IncuTwin_FW/components/ui IncuTwin_FW/components/usage_stats IncuTwin_FW/components/cloud_status IncuTwin_FW/components/affection IncuTwin_FW/test_apps/main
git commit -m "feat(hmi): acariciar al bebe y hacerle cosquillitas en Inicio"
```

---

### Task 5: Avisar de lo que se abre (`ui_notice`, `ui_hearts`, `rewards`, consola)

**Files:**
- Modify: `components/rewards/include/rewards.h`, `components/rewards/rewards.c`
- Modify: `components/ui/ui_notice.c`, `components/ui/ui_notice.h` (si hace falta), `components/ui/ui.c`, `components/ui/ui_hearts.c`
- Modify: `components/ui/ui_i18n.h`, `components/ui/ui_i18n.c`
- Modify: `components/support_console/console_cmds.c`
- Modify: `openspec/changes/hmi-corazones/specs/twin-display/spec.md`

**Interfaces:**
- Consumes: `rw_catchup_due`, `rw_catchup_done`, `rw_level_t` (tarea 1).
- Produces:
  - `bool rewards_catchup_take(rw_level_t *level);` — con el estado de verdad (nunca el de la demo) y bajo el mutex: si hay aviso pendiente devuelve true y el nivel, marca `rw_catchup_done` y guarda.
  - `void rewards_demo_set_level(rw_level_t level);` — solo con la demo activa: pone `earned` de la demo en `rw_level_min(level)`.
  - `void ui_level_card_show(rw_level_t level, bool catchup);` — la tarjeta de subida de nivel.
  - Textos (ES / EN):
    - `STR_AFF_OPEN_CARESS`: «Ahora puedes acariciarle: pasa el dedo sobre el bebé» / «You can now caress the baby: slide your finger over them»
    - `STR_AFF_OPEN_TICKLES`: «Ahora puedes hacerle cosquillitas: acaríciale rápido» / «You can now tickle the baby: stroke quickly»
    - `STR_AFF_CATCHUP`: «Tus corazones ya abren esto:» / «Your hearts already unlock this:»
    - `STR_AFF_HOW_HAND`: «Manita: pulsa "Coge mi mano" y pon tu dedo» / «Little hand: tap "Hold my hand" and put your finger»
    - `STR_AFF_HOW_CARESS`: «Caricia: pasa el dedo sobre el bebé» / «Caress: slide your finger over the baby»
    - `STR_AFF_HOW_TICKLES`: «Cosquillitas: acaríciale rápido» / «Tickles: stroke quickly»
    - `STR_AFF_NEXT_OPENS`: «%s abre: %s» / «%s unlocks: %s» (nivel siguiente, nombre corto de la interacción: «acariciarle» / «caress», «cosquillitas» / «tickles»)
    - `STR_AFF_OK`: «Vale» / «OK»

- [ ] **Step 1: Tarjeta de subida de nivel**

Hoy `Q_LEVEL` pinta «¡Subes a %s!» en la franja de 44 px durante 4 s. Para los niveles que abren algo en esta fase (Caricia y Cosquillitas) pasa a ser una tarjeta centrada sobre la pantalla, con el mismo estilo que las tarjetas de Corazones:
- Título: «¡Subes a Caricia!» (`STR_RW_LEVEL_UP`), o `STR_AFF_CATCHUP` si es el aviso pendiente.
- Texto: `STR_AFF_OPEN_CARESS` o `STR_AFF_OPEN_TICKLES`. En el aviso pendiente de alguien en Cosquillitas o más, las dos líneas.
- Botón `STR_AFF_OK`. Se queda hasta tocar el botón o la tarjeta. No tapa un «Coge mi mano» en marcha: espera a `HH_IDLE`, como el resto de avisos.
- Para Abrazo y Achuchón (sin interacción en esta fase) se mantiene la franja de hoy.

- [ ] **Step 2: Aviso pendiente al arrancar**

En `ui.c`, una vez con la pantalla de Inicio lista y sin demo: `if (rewards_catchup_take(&lv)) ui_level_card_show(lv, true);` solo si `lv >= RW_LEVEL_CARESS`.

- [ ] **Step 3: Tarjeta del nivel en Corazones**

En `level_card()` de `ui_hearts.c`: bajo la barra, una línea `STR_AFF_NEXT_OPENS` cuando el siguiente nivel es Caricia o Cosquillitas (para Abrazo y Achuchón, nada en esta fase). Al tocar la tarjeta se abre una burbuja o tarjeta con las líneas `STR_AFF_HOW_*` de lo que ya está abierto (siempre Manita; Caricia y Cosquillitas según el nivel).

- [ ] **Step 4: Consola**

`rewards nivel <0-4>`: solo en la demo (fuera de ella responde que solo vale en la demo, como `rewards comprar`). Llama a `rewards_demo_set_level()`.

- [ ] **Step 5: Spec de pantalla**

En `openspec/changes/hmi-corazones/specs/twin-display/spec.md`: requisito nuevo «Afecto» con los escenarios de caricia, cosquillitas, dedo quieto, tarjeta de subida, aviso pendiente único y «no cuenta corazones».

- [ ] **Step 6: Probar en la placa**

Avisar a Pablo, flashear, y:
1. `demo on`, `rewards nivel 0`, y un gesto de demo hasta pasar de 9 a 10 (o `rewards nivel 1` y mirar Corazones): sale la tarjeta «¡Subes a Caricia!» con el texto y el botón; se queda más de 10 s; se va al tocar.
2. Corazones en nivel Manita de demo: «Caricia abre: acariciarle». Tocar la tarjeta: lista con Manita.
3. `rewards nivel 2`: la lista enseña Manita, Caricia y Cosquillitas; acariciar rápido hace cosquillas.
4. Aviso pendiente: en una unidad cuyo guardado de verdad tenga 10 o más ganados, el primer arranque enseña la tarjeta `STR_AFF_CATCHUP`; tras tocar «Vale» y reiniciar, no vuelve a salir. Con `demo on` desde el arranque no sale ni se gasta.
5. Capturas de 1, 2 y 4 para Pablo.
Expected: los cuatro casos así; `idf.py -C test_apps` sigue con 0 fallos.

- [ ] **Step 7: Commit**

```bash
git add IncuTwin_FW/components IncuTwin_FW/openspec/changes/hmi-corazones/specs/twin-display/spec.md
git commit -m "feat(hmi): aviso de lo que abre cada nivel y como se hace"
```

---

## Al terminar

- Pasar `idf.py build` de los productos `dev` y `sim` (ver README) para comprobar que compilan.
- Enseñar a Pablo las capturas finales. Publicar el firmware es decisión suya: este plan no sube versión ni hace release.
- Apuntar en `C:\Users\Pablo\Documents\IncuTwin\incutwin-app\docs\incutwin\` (referencia para la app) los nombres y umbrales nuevos solo si Pablo lo pide: la paridad con la app es un no objetivo de este cambio.
