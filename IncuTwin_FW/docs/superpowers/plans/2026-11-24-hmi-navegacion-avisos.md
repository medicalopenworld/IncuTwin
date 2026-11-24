# Navegación y avisos en la IncuTwin — plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** barra fija con Inicio / 🐣 Historial / Ajustes, Historial de los 50 últimos avisos en NVS, aviso en pantalla
por cada cambio de lo que vive el bebé, buzzer solo en ingreso y «con sus papás», horario nocturno configurable.

**Architecture:** el modelo (`twin_model`) calcula los avisos con una función pura contra el último estado en línea y
publica `TWIN_EVT_NOTICE` (sustituye a `TWIN_EVT_TRANSITION`). Tres consumidores: `event_log` (anillo en NVS),
`sound` (melodías con horario nocturno) y la UI (franja de aviso y punto de no visto). La UI pasa a cuatro pantallas
(splash, home, history, settings) con una barra de navegación creada en cada una.

**Tech Stack:** ESP-IDF 6.0.1, LVGL 9.6 (`esp_lvgl_port`), Unity en placa (`test_apps`), Python + PIL para assets.

**Spec:** `IncuTwin_FW/docs/superpowers/specs/2026-11-24-hmi-navegacion-avisos-design.md`

Todas las rutas son relativas a `IncuTwin_FW/`. Entorno: en PowerShell, `. C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1` (el `export.ps1` del
README falla en este equipo: no encuentra el venv de Python).
**La placa de pruebas está en COM6** (no COM12 como dice el README).

## Global Constraints

- Textos nuevos en `components/ui/ui_i18n.c` en español e inglés a la vez; textos de aviso exactamente los de la tabla
  de la spec.
- Sin avisos de conexión, `thermo = alarm`, pulsioxímetro, nombre ni sexo. Salida en tono neutro y sin buzzer.
- Buzzer: `SOUND_BABY` en `ADMISSION`, `SOUND_PARENTS` en `PARENTS`, nada más; ninguno dentro del horario nocturno.
- Horario nocturno por defecto: activado, desde 21, hasta 9 (horas enteras), zona `CET-1CEST,M3.5.0,M10.5.0/3`.
- Historial: 50 entradas máximo, NVS namespace `evlog`.
- Barra de navegación: 40 px de alto en `y = 280`; ningún elemento de Inicio ni de Ajustes por debajo de `y = 280`.
- Los handlers del bus de eventos no tocan LVGL (copian y marcan *dirty*; la UI aplica en su `lv_timer`).
- `idf.py build` sin avisos nuevos del compilador; los tests de `test_apps` pasan en la placa de COM6.

## Review Focus

1. **Reinicio con bebé dentro y calor:** el primer estado tras arrancar no da `HEAT_ON` ni `ADMISSION` → tarea 1.
2. **La IncuNest se desconecta y vuelve igual:** ningún aviso; si vuelve sin bebé, un solo `EXIT` → tarea 1.
3. **Horario que cruza la medianoche y placa sin hora SNTP:** sin hora no se considera noche (suena) → tarea 3.
4. **51 avisos:** se conserva el más reciente y se pierde el más antiguo; el contador de no vistos no pasa de 50 → tarea 2.
5. **Cambio de IncuNest emparejada o en acompañar:** el estado de la nueva no genera avisos → tarea 4.

---

### Task 1: Avisos como función pura (`twin_notify`)

**Files:**
- Create: `components/twin_model/twin_notify.c`, `components/twin_model/include/twin_notify.h`
- Modify: `components/twin_types/include/twin_types.h` (tipo `twin_notice_t`), `components/twin_model/CMakeLists.txt` (SRCS)
- Test: `test_apps/main/test_twin_notify.c` (añadir a `test_apps/main/CMakeLists.txt`)

**Interfaces:**
- Produces:
  - En `twin_types.h`: `typedef enum { TWIN_NOTICE_ADMISSION = 1u << 0, TWIN_NOTICE_PARENTS = 1u << 1, TWIN_NOTICE_HEAT_ON = 1u << 2, TWIN_NOTICE_HEAT_OFF = 1u << 3, TWIN_NOTICE_PHOTO_ON = 1u << 4, TWIN_NOTICE_PHOTO_OFF = 1u << 5, TWIN_NOTICE_EXIT = 1u << 6 } twin_notice_t;` (el orden de los bits es el orden en que se muestran y guardan)
  - `typedef struct { bool valid; bool treatments_known; twin_baby_t baby; bool heat; bool photo; } twin_notice_base_t;`
  - `void twin_notice_base_boot(twin_notice_base_t *b, bool has_baby, twin_baby_t baby);` — `valid = has_baby`, `treatments_known = false`
  - `uint32_t twin_notices_update(twin_notice_base_t *b, const twin_incubator_t *now);`

Reglas de `twin_notices_update` (`heat` = `thermo != TWIN_THERMO_OFF`; «dentro» = `in` o `parents`):
- `!now->online` → `0` y la base no cambia.
- `!b->valid` → copiar `now` a la base, `valid = treatments_known = true`, devolver `0`.
- Fuera → dentro (`none`/`home` → `in`/`parents`): `ADMISSION`. `in` → `parents`: `PARENTS`. Dentro → fuera: `EXIT`. `parents` → `in`: nada.
- Si `treatments_known`: `heat` y `photo` dan `*_ON` / `*_OFF` al cambiar. Si no, no dan nada y pasa a `true`.
- Siempre copia `baby`, `heat`, `photo` de `now` a la base.

- [ ] **Step 1: Write the failing tests** (helper `inc(baby, thermo, photo, online)` sobre `twin_incubator_reset`):

```c
TEST_CASE("notice: primer estado solo fija la base", "[notice]") {
    twin_notice_base_t b = {0};
    TEST_ASSERT_EQUAL_UINT32(0, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_STABLE, true, true)));
}
TEST_CASE("notice: arranque con bebe guardado no avisa de calor", "[notice]") {
    twin_notice_base_t b; twin_notice_base_boot(&b, true, TWIN_BABY_IN);
    TEST_ASSERT_EQUAL_UINT32(0, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_STABLE, true, true)));
    TEST_ASSERT_EQUAL_UINT32(TWIN_NOTICE_PHOTO_OFF, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_STABLE, false, true)));
}
TEST_CASE("notice: arranque sin bebe guardado y llega bebe = ingreso", "[notice]") {
    twin_notice_base_t b; twin_notice_base_boot(&b, true, TWIN_BABY_NONE);
    TEST_ASSERT_EQUAL_UINT32(TWIN_NOTICE_ADMISSION, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_HEATING, false, true)));
}
TEST_CASE("notice: ingreso con calor", "[notice]") {
    twin_notice_base_t b = base(TWIN_BABY_NONE, false, false);
    TEST_ASSERT_EQUAL_UINT32(TWIN_NOTICE_ADMISSION | TWIN_NOTICE_HEAT_ON, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_HEATING, false, true)));
}
TEST_CASE("notice: in->parents avisa, parents->in no", "[notice]") {
    twin_notice_base_t b = base(TWIN_BABY_IN, false, false);
    TEST_ASSERT_EQUAL_UINT32(TWIN_NOTICE_PARENTS, twin_notices_update(&b, inc(TWIN_BABY_PARENTS, TWIN_THERMO_OFF, false, true)));
    TEST_ASSERT_EQUAL_UINT32(0, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_OFF, false, true)));
}
TEST_CASE("notice: home es salida, sin fanfarria", "[notice]") {
    twin_notice_base_t b = base(TWIN_BABY_PARENTS, false, false);
    TEST_ASSERT_EQUAL_UINT32(TWIN_NOTICE_EXIT, twin_notices_update(&b, inc(TWIN_BABY_HOME, TWIN_THERMO_OFF, false, true)));
}
TEST_CASE("notice: offline no avisa y al volver igual tampoco", "[notice]") {
    twin_notice_base_t b = base(TWIN_BABY_IN, true, false);
    TEST_ASSERT_EQUAL_UINT32(0, twin_notices_update(&b, inc(TWIN_BABY_NONE, TWIN_THERMO_OFF, false, false)));
    TEST_ASSERT_EQUAL_UINT32(0, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_STABLE, false, true)));
}
TEST_CASE("notice: vuelve de offline sin bebe = una salida", "[notice]") {
    twin_notice_base_t b = base(TWIN_BABY_IN, false, false);
    twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_OFF, false, false));
    TEST_ASSERT_EQUAL_UINT32(TWIN_NOTICE_EXIT, twin_notices_update(&b, inc(TWIN_BABY_NONE, TWIN_THERMO_OFF, false, true)));
}
TEST_CASE("notice: alarma con calor previo no avisa", "[notice]") {
    twin_notice_base_t b = base(TWIN_BABY_IN, true, false);
    TEST_ASSERT_EQUAL_UINT32(0, twin_notices_update(&b, inc(TWIN_BABY_IN, TWIN_THERMO_ALARM, false, true)));
}
```
  `base(baby, heat, photo)` devuelve una base `valid` y `treatments_known`. `inc(baby, thermo, photo, online)` devuelve
  `const twin_incubator_t *` a un `static` del test (en C no se puede tomar la dirección de un valor devuelto).

- [ ] **Step 2: Run** `idf.py -C test_apps -p COM6 flash` y `python tools/serial_capture.py --port COM6 --seconds 20 --reset` — Expected: no compila (`twin_notices_update` no existe).
- [ ] **Step 3: Implement** según las reglas.
- [ ] **Step 4: Run** lo mismo — Expected: `0 Failures` en el resumen de Unity.
- [ ] **Step 5: Commit** `git commit -m "feat(twin): avisos de lo que vive el bebe como funcion pura"`

---

### Task 2: Historial en NVS (`event_log`)

**Files:**
- Create: `components/event_log/{CMakeLists.txt, event_ring.c, event_log.c, include/event_ring.h, include/event_log.h}`
- Modify: `components/storage/include/storage.h` (`#define STORAGE_NS_EVLOG "evlog"`), `components/app_events/include/app_events.h` (`TWIN_EVT_LOG_CHANGED`, sin datos), `main/` (llamar a `event_log_init()` tras `storage_init()`)
- Test: `test_apps/main/test_event_ring.c` (compilar `event_ring.c` en el test app como los demás módulos puros)

**Interfaces:**
- Consumes: `twin_notice_t` (tarea 1)
- Produces:
  - `#define EVENT_LOG_MAX 50`
  - `typedef struct { uint32_t at; uint8_t kind; char inc[TWIN_INCUBATOR_ID_LEN]; } event_entry_t;` (`kind` = un solo bit de `twin_notice_t`)
  - `typedef struct { uint8_t head; uint8_t count; uint8_t unseen; event_entry_t e[EVENT_LOG_MAX]; } event_ring_t;`
  - `void event_ring_push(event_ring_t *r, const event_entry_t *e)`; `bool event_ring_get(const event_ring_t *r, uint8_t i, event_entry_t *out)` (`i = 0` el más reciente); `void event_ring_mark_seen(event_ring_t *r)`
  - `void event_log_init(void)` (carga el blob `ring` de `evlog`; si no existe o mide distinto, anillo vacío); `void event_log_add(uint32_t notices_mask, const char *inc)` (una entrada por bit, en orden de bit; `at = time(NULL)` si es ≥ 1700000000, si no `0`; guarda el blob y publica `TWIN_EVT_LOG_CHANGED`); `uint8_t event_log_count(void)`; `bool event_log_get(uint8_t i, event_entry_t *out)`; `uint8_t event_log_unseen(void)`; `void event_log_mark_seen(void)` — todas con mutex.
  - `event_log` se suscribe a `TWIN_EVT_NOTICE` (tarea 4) e ignora los avisos con `demo = true`.

- [ ] **Step 1: Write the failing tests**

```c
TEST_CASE("ring: el mas reciente primero", "[evlog]") {
    event_ring_t r = {0};
    event_ring_push(&r, E(100, TWIN_NOTICE_ADMISSION)); event_ring_push(&r, E(200, TWIN_NOTICE_HEAT_ON));
    event_entry_t o; TEST_ASSERT_TRUE(event_ring_get(&r, 0, &o));
    TEST_ASSERT_EQUAL_UINT32(200, o.at); TEST_ASSERT_EQUAL_UINT8(2, r.unseen);
}
TEST_CASE("ring: 51 entradas conservan las 50 mas nuevas", "[evlog]") {
    event_ring_t r = {0};
    for (uint32_t i = 1; i <= 51; i++) event_ring_push(&r, E(i, TWIN_NOTICE_EXIT));
    event_entry_t o;
    TEST_ASSERT_EQUAL_UINT8(50, r.count); TEST_ASSERT_EQUAL_UINT8(50, r.unseen);
    event_ring_get(&r, 0, &o); TEST_ASSERT_EQUAL_UINT32(51, o.at);
    event_ring_get(&r, 49, &o); TEST_ASSERT_EQUAL_UINT32(2, o.at);
    TEST_ASSERT_FALSE(event_ring_get(&r, 50, &o));
}
TEST_CASE("ring: marcar visto", "[evlog]") {
    event_ring_t r = {0}; event_ring_push(&r, E(1, TWIN_NOTICE_EXIT));
    event_ring_mark_seen(&r); TEST_ASSERT_EQUAL_UINT8(0, r.unseen);
}
```

  `E(at, kind)` devuelve `const event_entry_t *` a un `static` del test con `inc = "SIM01"`.

- [ ] **Step 2: Run** el test app en COM6 — Expected: no compila.
- [ ] **Step 3: Implement** `event_ring.c`, `event_log.c` y el componente.
- [ ] **Step 4: Run** test app en COM6 — Expected: `0 Failures`. `idf.py build` — Expected: compila.
- [ ] **Step 5: Commit** `git commit -m "feat(evlog): historial de los 50 ultimos avisos en NVS"`

---

### Task 3: Horario nocturno en Ajustes

**Files:**
- Create: `components/settings/quiet_hours.c`, `components/settings/include/quiet_hours.h`
- Modify: `components/settings/settings.c`, `components/settings/include/settings.h`, `components/app_events/include/app_events.h` (`app_evt_settings_t`), `components/accompany/accompany.c`, `components/accompany/acc_logic.c`, `components/accompany/include/acc_logic.h`, `test_apps/main/test_accompany.c`, `main/` (TZ al arrancar)
- Test: `test_apps/main/test_quiet_hours.c`

**Interfaces:**
- Produces:
  - `bool quiet_hours_active(int hour, bool on, uint8_t from, uint8_t to)` — `hour < 0` (sin hora) → `false`; `from == to` → `false`; `from > to` cruza la medianoche; intervalo `[from, to)`.
  - `settings_quiet_on()`, `settings_quiet_from()`, `settings_quiet_to()` y sus `settings_set_*`; defectos `1`, `21`, `9`; horas fuera de 0–23 → defecto. Claves NVS `q_on`, `q_from`, `q_to`.
  - `app_evt_settings_t` añade `uint8_t quiet_on, quiet_from, quiet_to;`
  - `int settings_local_hour(void)` — `localtime_r` de `time(NULL)`; `-1` si la hora es < 1700000000.
  - El `setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3")` + `tzset()` se hace en el arranque de `main`, no en `accompany.c`.
  - `accompany.c` usa `quiet_hours_active(settings_local_hour(), settings_quiet_on(), settings_quiet_from(), settings_quiet_to())`; se borran `ACC_QUIET_FROM`, `ACC_QUIET_TO`, `acc_quiet()` y sus tests.

- [ ] **Step 1: Write the failing tests**

```c
TEST_CASE("quiet: 21-9 por defecto", "[quiet]") {
    TEST_ASSERT_FALSE(quiet_hours_active(20, true, 21, 9));
    TEST_ASSERT_TRUE(quiet_hours_active(21, true, 21, 9));
    TEST_ASSERT_TRUE(quiet_hours_active(0, true, 21, 9));
    TEST_ASSERT_TRUE(quiet_hours_active(8, true, 21, 9));
    TEST_ASSERT_FALSE(quiet_hours_active(9, true, 21, 9));
}
TEST_CASE("quiet: apagado, sin hora o from==to nunca", "[quiet]") {
    TEST_ASSERT_FALSE(quiet_hours_active(23, false, 21, 9));
    TEST_ASSERT_FALSE(quiet_hours_active(-1, true, 21, 9));
    TEST_ASSERT_FALSE(quiet_hours_active(10, true, 10, 10));
}
TEST_CASE("quiet: intervalo sin medianoche", "[quiet]") {
    TEST_ASSERT_TRUE(quiet_hours_active(14, true, 13, 15));
    TEST_ASSERT_FALSE(quiet_hours_active(15, true, 13, 15));
}
```

- [ ] **Step 2: Run** test app en COM6 — Expected: no compila.
- [ ] **Step 3: Implement** lo de *Produces*.
- [ ] **Step 4: Run** test app en COM6 — Expected: `0 Failures` (también `[acc]`). `idf.py build` — Expected: compila.
- [ ] **Step 5: Commit** `git commit -m "feat(settings): horario nocturno configurable, 21 a 9 por defecto"`

---

### Task 4: Publicar los avisos y que el buzzer los use

**Files:**
- Modify: `components/twin_model/twin_model.c`, `components/twin_model/twin_logic.c`, `components/twin_model/include/twin_logic.h`, `components/twin_types/include/twin_types.h` (borrar `twin_transition_t`), `components/app_events/include/app_events.h`, `components/sound/sound.c`, `components/sound/include/sound.h`, `test_apps/main/test_twin_logic.c`

**Interfaces:**
- Consumes: tareas 1–3.
- Produces:
  - `TWIN_EVT_NOTICE` sustituye a `TWIN_EVT_TRANSITION`; data `typedef struct { uint32_t mask; bool demo; char inc[TWIN_INCUBATOR_ID_LEN]; } app_evt_notice_t;`
  - `twin_model.c`: `static twin_notice_base_t s_base, s_demo_base;` En `twin_model_init`, `twin_notice_base_boot(&s_base, <hay "baby" en NVS>, <su valor>)`. En `twin_model_ingest` (solo si `is_new`) y `twin_model_sim_apply`: `twin_notices_update(&s_base, &s_inc)`; en `twin_model_demo_apply`: con `s_demo_base` y `demo = true`; `twin_model_demo_enter` invalida `s_demo_base`. `twin_model_set_pairing` y cualquier cambio de incubadora del modo acompañar invalidan `s_base` (`valid = false`). Si `mask != 0`, publicar `TWIN_EVT_NOTICE` fuera del mutex.
  - Se borran `twin_transitions_between`, `post_transitions`, `TWIN_TR_*` y `r.transitions`; los tests de transiciones de `test_twin_logic.c` se borran (los cubre la tarea 1).
  - `sound.c`: escucha `TWIN_EVT_NOTICE`: `ADMISSION` → `SOUND_BABY`; si no, `PARENTS` → `SOUND_PARENTS`; nada más. Un `lv_timer` cada 30 s calcula `quiet_hours_active(settings_local_hour(), …)` y lo guarda en `s_quiet` (sustituye a `sound_set_quiet`, que se borra del API y de `accompany.c`). El clic y el latido al tocar no miran `s_quiet`.

- [ ] **Step 1: Run** test app en COM6 tras borrar los tests de transiciones — Expected: `0 Failures`.
- [ ] **Step 2: Implement** lo de *Produces*.
- [ ] **Step 3: Run** `idf.py build` — Expected: compila sin referencias a `TWIN_EVT_TRANSITION` (`git grep TWIN_EVT_TRANSITION` vacío).
- [ ] **Step 4: Run** build `sim` en COM6 (`idf.py -B build-sim -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.sim" -p COM6 flash`), abrir `SIM: http://<ip>/` del log y: bebé dentro → suena «bebé detectado»; quitar bebé → no suena; bebé dentro y luego «con sus papás» → fanfarria. (El horario nocturno se prueba en la tarea 8, cuando ya se puede cambiar desde Ajustes.) Anotar el resultado en el informe.
- [ ] **Step 5: Commit** `git commit -m "feat(twin): publicar avisos; el buzzer solo suena en ingreso y con sus papas"`

---

### Task 5: Iconos (pollito y barra)

**Files:**
- Modify: `tools/gen_extra.py` (o el generador que ya produce iconos sueltos), `components/assets/` (salida generada), `components/assets/include/assets.h`; si `LV_SYMBOL_HOME` no está en `lv_font_es_16`, añadirlo en `tools/gen_fonts.py` y regenerar.

**Interfaces:**
- Produces: `img_chick_28` (28×28, ARGB8888) y `img_chick_64` (64×64) declarados en `assets.h`: pollito amarillo `#F6C443` asomando de media cáscara blanca con borde en zigzag, ojo y pico naranja `#F28C28`, dibujado con PIL (sin imágenes de terceros); preview en `tools/preview/chick.png`. `LV_SYMBOL_HOME` y `LV_SYMBOL_SETTINGS` disponibles en `lv_font_es_16`.

- [ ] **Step 1:** Generar con el script y mirar `tools/preview/chick.png` (que se reconozca un pollito en 28 px).
- [ ] **Step 2: Run** `idf.py build` — Expected: compila.
- [ ] **Step 3: Commit** `git commit -m "feat(assets): pollito del historial y simbolo de inicio"`

---

### Task 6: Barra de navegación, pantalla de Historial y reordenar Inicio y Ajustes

**Files:**
- Create: `components/ui/ui_nav.c`, `components/ui/ui_history.c`
- Modify: `components/ui/ui.c`, `components/ui/ui_internal.h`, `components/ui/ui_home.c`, `components/ui/ui_settings.c`, `components/ui/ui_i18n.c`, `components/ui/CMakeLists.txt` (REQUIRES `event_log`)

**Interfaces:**
- Consumes: `event_log_*` (tarea 2), `settings_quiet_*` (tarea 3), `img_chick_*` (tarea 5)
- Produces:
  - `typedef enum { UI_TAB_HOME = 0, UI_TAB_HISTORY, UI_TAB_SETTINGS } ui_tab_t;`
  - `lv_obj_t *ui_nav_create(lv_obj_t *scr, ui_tab_t active)` — barra de 240×40 en `y = 280`, tres botones de 80×40 (casa, pollito 28 px, rueda); activo en `COL_NAVY` y opaco, inactivos al 50 %; punto coral de 8 px en la esquina superior derecha del pollito. Pulsación larga en Ajustes → el diálogo de borrado de fábrica que hoy cuelga de la rueda (mover `gear_long_pressed`).
  - `void ui_nav_set_unread(bool unread)` — actualiza el punto en las tres barras.
  - `void ui_go(ui_tab_t tab)` en `ui.c` — `lv_screen_load_anim` 220 ms, `MOVE_LEFT` si el índice sube y `MOVE_RIGHT` si baja; sustituye a `ui_go_settings`/`ui_go_home`. Al entrar en Historial, `event_log_mark_seen()`.
  - `ui.c` se suscribe a `TWIN_EVT_LOG_CHANGED` (flag *dirty*) y en `refresh_cb` llama a `ui_history_refresh()` y `ui_nav_set_unread(event_log_unseen() > 0)`.
  - `lv_obj_t *ui_history_create(void)`, `void ui_history_refresh(void)`, `void ui_history_retranslate(void)` — título «Historial» / «History»; lista con scroll vertical en `y` 40–280; fila: icono (`LV_SYMBOL` o asset por tipo), texto del aviso y tiempo relativo (`ui_rel_time(uint32_t at, uint32_t now, char *out, size_t n)`: «ahora», «hace N min», «hace N h», «hace N d» / «now», «N min ago», «N h ago», «N d ago»; `at = 0` → «—»). Vacía: `img_chick_64` y «Aquí verás lo que vaya pasando en tu IncuNest» / «Here you'll see what happens in your IncuNest».
  - Inicio: fuera la rueda dentada; fila de iconos, «Coge mi mano» y barra de estado recolocadas para que nada quede por debajo de `y = 280` ni se solape con el bebé ni el nombre (proponer `ICON_Y` y `STATUS_Y` nuevos y, si no cabe, reducir `ICON_D` a 40). Lo mismo en las páginas de la demo (`ui_pages.c`, `DOTS_Y`).
  - Ajustes: fuera el botón «atrás»; título arriba; nueva sección «De noche, sin sonido» / «Quiet at night» con un interruptor y dos selectores de hora «Desde» / «Hasta» (`lv_roller` 00–23 o botones −/+), que llaman a `settings_set_quiet_*`; el pie (red y versión) encima de la barra.
  - Textos de los avisos en `ui_i18n.c` con claves `STR_NOTICE_ADMISSION`, `STR_NOTICE_PARENTS`, `STR_NOTICE_EXIT`, `STR_NOTICE_HEAT_ON`, `STR_NOTICE_HEAT_OFF`, `STR_NOTICE_PHOTO_ON`, `STR_NOTICE_PHOTO_OFF` (exactos de la spec), y `const char *ui_notice_text(twin_notice_t kind)`.

- [ ] **Step 1: Write the failing test** de `ui_rel_time` (función pura en `components/ui/ui_rel_time.c`, compilada en el test app):

```c
TEST_CASE("rel_time: tramos", "[ui]") {
    char b[24];
    ui_rel_time(1000, 1030, b, sizeof b, 0); TEST_ASSERT_EQUAL_STRING("ahora", b);
    ui_rel_time(1000, 1000 + 5 * 60, b, sizeof b, 0); TEST_ASSERT_EQUAL_STRING("hace 5 min", b);
    ui_rel_time(1000, 1000 + 2 * 3600, b, sizeof b, 0); TEST_ASSERT_EQUAL_STRING("hace 2 h", b);
    ui_rel_time(1000, 1000 + 3 * 86400, b, sizeof b, 1); TEST_ASSERT_EQUAL_STRING("3 d ago", b);
    ui_rel_time(0, 5000, b, sizeof b, 0); TEST_ASSERT_EQUAL_STRING("—", b);
}
```
  (último parámetro: idioma `SETTINGS_LANG_ES`/`EN`; «ahora» por debajo de 60 s.)

- [ ] **Step 2: Run** test app en COM6 — Expected: no compila.
- [ ] **Step 3: Implement** lo de *Produces*.
- [ ] **Step 4: Run** test app en COM6 (`0 Failures`) y `idf.py build`.
- [ ] **Step 5: Run** build `sim` en COM6 y comprobar a mano: los tres botones cambian de pantalla; nada de Inicio ni de Ajustes queda tapado; «Coge mi mano» se pulsa; pulsación larga en Ajustes abre el borrado de fábrica (cancelar); Historial vacío muestra el pollito. Hacer una foto o describir la pantalla en el informe.
- [ ] **Step 6: Commit** `git commit -m "feat(hmi): barra de navegacion, historial y ajustes de noche"`

---

### Task 7: Franja de aviso en pantalla

**Files:**
- Create: `components/ui/ui_notice.c`
- Modify: `components/ui/ui.c`, `components/ui/ui_internal.h`

**Interfaces:**
- Consumes: `TWIN_EVT_NOTICE`, `ui_notice_text`, `ui_go`
- Produces: `void ui_notice_init(void)` (crea la franja en `lv_layer_top()`, oculta); `void ui_notice_push(uint32_t mask)` (encola un aviso por bit, en orden de bit, cola de 8; se llama desde `refresh_cb` con lo copiado del evento). Franja de 224×44 en `x = 8`, baja de `y = -48` a `y = 4` en 200 ms, se queda 4 s y sube; tocarla → `ui_go(UI_TAB_HISTORY)` y vacía la cola. No aparece en el splash, el onboarding, la despedida ni mientras está abierto el aviso o el teclado del nombre del bebé (`ui_name_open()`, `components/ui/ui_name.c`): se encola y sale al cerrarse.

- [ ] **Step 1: Implement** lo de *Produces* (sin test unitario: es LVGL).
- [ ] **Step 2: Run** `idf.py build`.
- [ ] **Step 3: Run** build `sim` en COM6: bebé dentro con calor → dos franjas seguidas («Ha entrado un bebé…», «Ha empezado el calor») y suena una vez; tocar una abre el Historial con las dos filas y el punto desaparece; quitar el bebé → franja de salida, sin sonido.
- [ ] **Step 4: Commit** `git commit -m "feat(hmi): aviso en pantalla de cada cambio del bebe"`

---

### Task 8: Specs, prueba completa en COM6 y build de producto

**Files:**
- Modify: `openspec/specs/twin-display/spec.md`, `openspec/specs/sound/spec.md`, `openspec/specs/settings/spec.md`; Create: `openspec/specs/event-history/spec.md` (requisitos de esta spec en el formato de las demás, con *Pendiente: varias IncuNest por IncuTwin*).

- [ ] **Step 1:** Actualizar las specs (navegación, historial, avisos, horario nocturno, cambios de comportamiento: `home` sin fanfarria, `parents → in` sin melodía).
- [ ] **Step 2: Run** test app completo en COM6 — Expected: `0 Failures`.
- [ ] **Step 3: Run** build `sim` en COM6 y recorrer: ingreso → fototerapia → canguro → vuelta → alta; desconectar y reconectar con bebé; reiniciar la placa con bebé y calor (ningún aviso al arrancar); horario nocturno con la hora actual dentro (franja sí, sonido no); 51 avisos con la demo automática del sim (el Historial no pasa de 50).
- [ ] **Step 4: Run** `idf.py build` y `idf.py -p COM6 flash` para dejar la placa con la build de producto.
- [ ] **Step 5: Commit** `git commit -m "docs(spec): navegacion, historial y avisos en la pantalla"`
