# Nombre del bebé puesto en la IncuTwin: plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** cuando a la incubadora que sigue la IncuTwin llega un bebé sin nombre, el panel pregunta si quieres ponérselo; el nombre se guarda solo en la IncuTwin.

**Architecture:** ThingsBoard añade `stay_id` al estado del broker y deja de mandar el apodo como `name`. El firmware parsea `stay_id`. Un componente nuevo `baby_name` decide la estancia (con `stay_id` o, si no llega, un contador de llegadas por incubadora), cuándo preguntar y qué nombre mostrar: lógica pura en `name_logic.c` con tests on-target y un blob en NVS en `baby_name.c`. La UI añade el aviso y el teclado (`ui_name.c`) y los engancha en `ui_home.c`.

**Tech Stack:** ESP-IDF 6.0.1, LVGL 9, Unity (`test_apps`), ThingsBoard rule-node JS (Nashorn) con tests en Node.

**Spec:** `docs/superpowers/specs/2026-11-24-nombre-bebe-panel-design.md`

**Worktree:** `C:\Users\Pablo\Documents\IncuTwin-sexo`, rama `test/baby-sex-acompanar`. Rutas relativas a `IncuTwin_FW/` salvo que se diga otra cosa.

## Global Constraints

- El nombre local nunca sale del panel: ni MQTT, ni ThingsBoard, ni `ESP_LOG*`, ni consola, ni `usage`.
- Máximo 12 caracteres (puntos de código UTF-8), sin espacios en los extremos; vacío equivale a cancelar.
- Teclado en español: letras, Ñ, Á É Í Ó Ú y espacio.
- Prioridad: nombre real (`name` del estado, solo con `itw_show_name`) > nombre local > «Ponle nombre».
- Pregunta solo con `baby = in`, sin nombre real, fuera de demo, una vez por estancia; espera si "Coge mi mano" no está en reposo o hay alarma (`TWIN_THERMO_ALARM`).
- Anillo de 8 estancias en NVS, namespace `bname`, borrado por `factory-reset`.
- Textos ES/EN en `ui_i18n` (el panel solo tiene esos dos idiomas).
- Rule nodes en Nashorn: todo `for`/`while` con llaves.
- Desplegar en ThingsBoard de producción **no** forma parte de este plan: pedir permiso a Pablo aparte.

## Review Focus

1. Canguro: `in → parents → in` es el mismo bebé; no vuelve a preguntar ni pierde el nombre (Task 3, `kangaroo_keeps_stay`).
2. Reinicio a mitad de estancia: el primer estado tras arrancar ya trae `baby = in`; mismo bebé, no pregunta (Task 3, `reboot_mid_stay`).
3. Modo Acompañar salta A → B → A: cada incubadora conserva su nombre; con más de 8 estancias se va la más antigua (Task 3, `two_incubators` y `ring_evicts_oldest`).
4. `stay_id` aparece a mitad de estancia (ThingsBoard desplegado con el bebé dentro): la entrada del contador pasa a la clave `stay_id` sin volver a preguntar (Task 3, `stay_id_adopts_fallback`).
5. Nombres raros: «  José María de la Luz  », 14 «Ñ», caracteres de 3 bytes, solo espacios: recorte a 12 puntos de código y a `BN_NAME_BYTES` sin partir UTF-8; solo espacios cancela (Task 3, `clean_name_*`).

---

### Task 0: Commit de la fusión

**Files:** todo lo resuelto en la fusión de `origin/claude/tender-fermat-tpve71` sobre `feat/acompanar`, más `docs/superpowers/` en la raíz del repo.

- [ ] **Step 1:** `git -C C:/Users/Pablo/Documents/IncuTwin-sexo status --short | grep -E '^(UU|AA)'` → sin salida (ningún conflicto sin marcar). Si hay, `git add` de esos ficheros tras comprobar que no quedan marcadores `<<<<<<<`.
- [ ] **Step 2:** `idf.py build` en `IncuTwin_FW` → `Project build complete`.
- [ ] **Step 3:** Commit:

```bash
git add -A IncuTwin_FW docs/superpowers
git commit -m "merge: claude/tender-fermat-tpve71 (sexo del bebe) -> feat/acompanar

tb/test_sm_anon.js falla hasta la Task 1 (apodo en estancias anonimas)."
```

### Task 1: ThingsBoard manda `stay_id` y solo el nombre real

**Files:**
- Modify: `tb/itw_state_machine.js` (bloque `if (shown) { sex = st.id.sex; name = st.id.alias; ... }` y el objeto `bp` del broker)
- Test: `tb/test_identity.js`, `tb/test_sm_anon.js`

**Interfaces:**
- Produces: `incubators/<id>/state` con `stay_id` (string, p. ej. `"297:4"` o `"297:0-1794260594"`) cuando `st.stay_id` no es null; `name` solo con consentimiento y mismo bebé.

- [ ] **Step 1: Tests que fallan** en `tb/test_identity.js`. Cambiar los asserts que comparan `a.broker.name` / `a.panel.name` con el apodo para que comparen `a.attrs.itw_baby_alias`. Añadir:

```js
// sin consentimiento: el apodo no va en el nombre del broker ni del panel
assert.strictEqual(a.broker.name, undefined);
assert.strictEqual(a.panel.name, '');
assert.ok(isName(a.broker.sex, a.attrs.itw_baby_alias));
// la estancia viaja al panel
assert.strictEqual(a.broker.stay_id, a.attrs.itw_stay_id);
// con itw_show_name y el mismo bebé: el nombre real
assert.strictEqual(b.broker.name, 'Lucía');
```

`a` y `b` son los resultados que ya construye el fichero (sin y con `ss_itw_show_name: 'true'`); reutilizar sus helpers.

- [ ] **Step 2:** `node tb/test_identity.js` → FAIL en `a.broker.name`.
- [ ] **Step 3:** En `itw_state_machine.js`, `name` empieza en `''` y solo toma el nombre real (no `st.id.alias`); en `bp` añadir `if (st.stay_id) bp.stay_id = st.stay_id;`. `itw_baby_alias`, `idPayload` y los eventos del webhook no cambian.
- [ ] **Step 4:** `for t in tb/test*.js; do node $t; done` → todos OK, incluido `test_sm_anon.js` y el de «payload del broker sigue cabiendo en 512 bytes».
- [ ] **Step 5:** Commit `feat(tb): stay_id en el estado del broker; el apodo ya no va como nombre`.

### Task 2: El firmware lee `stay_id`

**Files:**
- Modify: `components/twin_types/include/twin_types.h`, `components/twin_model/include/twin_logic.h`, `components/twin_model/twin_parse.c`, `components/twin_model/twin_logic.c`
- Test: `test_apps/main/test_twin_parse.c`, `test_apps/main/test_twin_logic.c`

**Interfaces:**
- Produces: `#define TWIN_STAY_LEN 40`; `char stay_id[TWIN_STAY_LEN]` en `twin_incubator_t` (`""` = no llega); `bool has_stay; char stay_id[TWIN_STAY_LEN];` en `twin_msg_t`.

- [ ] **Step 1: Tests que fallan:**
  - `TEST_CASE("parse: stay_id", "[twin][parse]")`: `"stay_id":"297:4"` → `has_stay` y `"297:4"`; `"297:0-1794260594"` → aceptado. Ignorados sin invalidar el mensaje: `""`, `7`, `null`, 40 caracteres, `"297:4 x"` (espacio), `"297/4"`.
  - En `"apply: extensiones prevalecen; nombre se borra si no viene"`: con `has_stay` → `inc.stay_id == "297:4"`; el segundo mensaje sin él → `""`.
  - En `"reset: estado inicial de la incubadora"`: `inc.stay_id == ""`.
- [ ] **Step 2:** `idf.py -C test_apps build` → error de compilación (`has_stay` no existe).
- [ ] **Step 3:** Parsear `stay_id`: string de 1 a 39 bytes, solo `[A-Za-z0-9:_-]`. `twin_apply` lo copia si viene y lo vacía si no (igual que `name`).
- [ ] **Step 4:** Tests on-target (ver Task 6, Step 1) o, si se agrupan, `idf.py -C test_apps build` → OK.
- [ ] **Step 5:** Commit `feat(hmi): stay_id en el estado del gemelo`.

### Task 3: Lógica del nombre (pura)

**Files:**
- Create: `components/baby_name/name_logic.c`, `components/baby_name/include/name_logic.h`
- Test: `test_apps/main/test_name_logic.c`; añadir el `.c` de test y `${COMP}/baby_name/name_logic.c` a `SRCS` y `${COMP}/baby_name/include` a `INCLUDE_DIRS` en `test_apps/main/CMakeLists.txt`

**Interfaces:**
- Consumes: `twin_baby_t`, `TWIN_INCUBATOR_ID_LEN`, `TWIN_NAME_LEN`, `TWIN_STAY_LEN` (Task 2).
- Produces:

```c
#define BN_RING 8
#define BN_INCS 4                 /* contadores de llegada por incubadora */
#define BN_NAME_MAX_CHARS 12
#define BN_NAME_BYTES 25         /* 12 letras de 2 bytes (Ñ, tildes) + 0 */
#define BN_KEY_LEN TWIN_STAY_LEN  /* stay_id, o "<incubator_id>#<gen>" */
#define BN_STORE_VERSION 1

typedef struct { char key[BN_KEY_LEN]; char name[BN_NAME_BYTES]; bool asked; uint32_t used; } bn_entry_t;
typedef struct { char inc[TWIN_INCUBATOR_ID_LEN]; uint16_t gen; bool was_in; uint32_t used; } bn_gen_t;
typedef struct { uint8_t version; uint32_t clock; bn_entry_t e[BN_RING]; bn_gen_t g[BN_INCS]; } bn_store_t;

void bn_store_reset(bn_store_t *st);
/* Actualiza contadores y deja en key_out la clave de la estancia ("" si no hay bebe dentro ni con
 * sus papas). Devuelve true si el store cambio (hay que guardarlo). */
bool bn_observe(bn_store_t *st, const char *incubator_id, const char *stay_id, twin_baby_t baby,
                char key_out[BN_KEY_LEN]);
const char *bn_name(const bn_store_t *st, const char *key);   /* "" si no hay */
bool bn_should_ask(const bn_store_t *st, const char *key);   /* false con key "" */
bool bn_mark_asked(bn_store_t *st, const char *key);         /* true si cambio */
bool bn_set_name(bn_store_t *st, const char *key, const char *raw); /* limpia; vacio = no cambia */
bool bn_clean_name(const char *raw, char out[BN_NAME_BYTES]);       /* false si queda vacio */
```

Reglas que fijan los tests: la clave es `stay_id` si no está vacío; si no, `"<incubator_id>#<gen>"`. `gen` sube solo al pasar a `TWIN_BABY_IN` desde algo que no es `IN` ni `PARENTS`; la primera vez que se ve una incubadora empieza en 1 sin contar como llegada. `bn_observe` con `stay_id` nuevo y una entrada del contador de esa incubadora con `asked` o nombre: le cambia la clave a `stay_id`. Al llenarse el anillo (o los contadores) se reemplaza la de `used` más bajo; `clock` sube en cada uso.

- [ ] **Step 1: Tests que fallan** (`[baby_name]`), cada uno con su nombre:
  - `first_baby_asks`: incubadora `"297"`, sin `stay_id`, `IN` → clave `"297#1"`, `bn_should_ask` true; tras `bn_mark_asked` → false.
  - `kangaroo_keeps_stay`: `IN`, `PARENTS`, `IN` → misma clave; con nombre `"Lola"` sigue `"Lola"` y no pregunta.
  - `new_arrival_new_stay`: `IN`, `NONE`, `IN` → `"297#2"`; pregunta; `bn_name` `""`.
  - `reboot_mid_stay`: store copiado con `memcpy` (como si viniera de NVS), primera observación `IN` → misma clave que antes, no pregunta.
  - `two_incubators`: `"297"` con `"Lola"`, `"327"` con `"Teo"`, vuelta a `"297"` → `"Lola"`.
  - `ring_evicts_oldest`: 9 estancias con nombre → la primera ya no tiene nombre, las 8 últimas sí.
  - `stay_id_keyed`: `stay_id "297:4"` → clave `"297:4"`; `bn_set_name` y vuelta → mismo nombre.
  - `stay_id_adopts_fallback`: `"297#1"` con `"Lola"`, luego la misma estancia con `stay_id "297:4"` → `bn_name == "Lola"`, no pregunta.
  - `no_baby_no_key`: `NONE` y `HOME` → `key_out ""`, `bn_should_ask("")` false.
  - `clean_name_trims_and_clips`: `"  José María de la Luz  "` → `"José María d"` (12 puntos de código); `"ÑÑÑÑÑÑÑÑÑÑÑÑÑÑ"` (14) → 12 `Ñ` (24 bytes); `"日本語日本語日本語日本"` (3 bytes cada uno) → corta sin partir caracteres y cabe en `BN_NAME_BYTES`; `"   "` → false.
  - `set_name_empty_keeps_previous`: con `"Lola"`, `bn_set_name(.., "  ")` → false y sigue `"Lola"`.
- [ ] **Step 2:** `idf.py -C test_apps build` → falla (no existe `name_logic.h`).
- [ ] **Step 3:** Implementar `name_logic.c` (sin ESP-IDF ni LVGL: solo `string.h`).
- [ ] **Step 4:** `idf.py -C test_apps build` → OK (se ejecutan en la Task 6).
- [ ] **Step 5:** Commit `feat(hmi): logica del nombre local del bebe`.

### Task 4: Guardado en NVS (`baby_name`)

**Files:**
- Create: `components/baby_name/baby_name.c`, `components/baby_name/include/baby_name.h`, `components/baby_name/CMakeLists.txt` (`SRCS baby_name.c name_logic.c`, `REQUIRES twin_types storage`)
- Modify: `components/storage/include/storage.h` (`#define STORAGE_NS_BNAME "bname"`), `components/storage/storage.c` (`storage_factory_reset` lo borra; actualizar el comentario de la cabecera)

**Interfaces:**
- Consumes: Task 3 entera; `storage_get_blob` / `storage_set_blob`.
- Produces (solo desde la tarea de la UI, con el lock de LVGL):

```c
void baby_name_init(void);                         /* carga el blob; version distinta = vacio */
void baby_name_observe(const twin_snapshot_t *s);  /* usa link.incubator_id, inc.stay_id, inc.baby */
bool baby_name_should_ask(void);                   /* estancia actual sin preguntar */
const char *baby_name_local(void);                 /* "" si no hay */
void baby_name_dismiss(void);                      /* AHORA NO */
bool baby_name_set(const char *raw);               /* guarda; false si queda vacio */
```

- [ ] **Step 1:** Implementar: blob `"store"` en `bname` con `bn_store_t`; guardar solo cuando una función de Task 3 devuelve true. Log permitido: `ESP_LOGI(TAG, "nombre local: %s", *name ? "si" : "no")`; nunca el nombre.
- [ ] **Step 2:** Llamar a `baby_name_init()` en `main/app_main.c` antes de crear la UI; añadir `baby_name` a los `REQUIRES` de `main` y de `ui`.
- [ ] **Step 3:** `idf.py build` → `Project build complete`.
- [ ] **Step 4:** `grep -rn 'name' components/baby_name/*.c | grep -i 'LOG'` → solo la línea de «si/no».
- [ ] **Step 5:** Commit `feat(hmi): nombre local del bebe en NVS (namespace bname)`.

### Task 5: Aviso y teclado (`ui_name`) y enganche en la pantalla del bebé

**Files:**
- Create: `components/ui/ui_name.c`, `components/ui/ui_name.h`
- Modify: `components/ui/CMakeLists.txt`, `components/ui/ui_i18n.h`, `components/ui/ui_i18n.c`, `components/ui/ui_home.c` (`ui_home_apply`, `overlay_refresh`, creación de `s_name` ~l.499)

**Interfaces:**
- Consumes: Task 4.
- Produces:

```c
void ui_name_create(lv_obj_t *scr);   /* capas ocultas encima de la pantalla del bebe */
void ui_name_ask(void);               /* tarjeta "Ha llegado un bebe" */
void ui_name_edit(void);              /* teclado, con el nombre local actual */
bool ui_name_open(void);              /* aviso o teclado visibles */
void ui_name_on_done(void (*cb)(void)); /* ui_home repinta el nombre al cerrar */
```

Textos (`STR_NAME_*`, ES / EN):

| Clave | ES | EN |
|---|---|---|
| `STR_NAME_ARRIVED` | Ha llegado un bebé | A baby has arrived |
| `STR_NAME_ASK` | ¿Quieres ponerle nombre? | Would you like to name them? |
| `STR_NAME_YES` | SÍ | YES |
| `STR_NAME_LATER` | AHORA NO | NOT NOW |
| `STR_NAME_PLACEHOLDER` | Ponle nombre | Give a name |
| `STR_NAME_DONE` | LISTO | DONE |

- [ ] **Step 1:** `ui_name.c`: tarjeta modal con el estilo de las tarjetas existentes (`COL_CARD`, borde `COL_CORAL`, `lv_font_es_16`). [SÍ] → `ui_name_edit`; [AHORA NO] → `baby_name_dismiss`. Teclado: `lv_textarea` (`max_length` 12, una línea) + `lv_keyboard` con mapa propio de 5 filas: `Q W E R T Y U I O P` / `A S D F G H J K L Ñ` / `Z X C V B N M ⌫` / `Á É Í Ó Ú` / `ESPACIO`, `LISTO`, `✕`. LISTO → `baby_name_set`; si devuelve false, se cierra sin cambios.
- [ ] **Step 2:** `ui_home_apply`: `baby_name_observe(s)` al principio. Nombre mostrado = `s->inc.name` si no está vacío; si no, `baby_name_local()`; si no, `STR_NAME_PLACEHOLDER` solo con `s->inc.baby == TWIN_BABY_IN` y `!s->demo`. `s_name` es clicable cuando no hay nombre real y no es demo → `ui_name_edit()`.
- [ ] **Step 3:** Preguntar (`ui_name_ask()`) cuando se cumplan a la vez: `baby_name_should_ask()`, `s->show_baby`, `s->inc.baby == TWIN_BABY_IN`, `s->inc.name[0] == '\0'`, `!s->demo`, `hh_state(&s_hh) == HH_IDLE`, `s->inc.thermo != TWIN_THERMO_ALARM`, `ui_pages_current() == 0`, `!ui_name_open()`. Comprobarlo en `ui_home_apply` y al volver a `HH_IDLE` en `hold_refresh`, para que pregunte cuando termine el gesto.
- [ ] **Step 4:** `idf.py build` → `Project build complete`, sin warnings nuevos.
- [ ] **Step 5:** Commit `feat(hmi): aviso y teclado para ponerle nombre al bebe`.

### Task 6: Tests on-target y prueba en la placa de COM6

- [ ] **Step 1: Tests on-target.** El `test_apps` usa la tabla de particiones por defecto, pero `nvs` está en `0x9000` igual que en el producto y los tests no tocan la NVS ni `otadata`. `idf.py -C test_apps -p COM6 flash`, luego `python tools/serial_capture.py --port COM6 --reset --seconds 25` → `0 Failures`, con los `[baby_name]` y los nuevos `[parse]`.
- [ ] **Step 2: Reflashear el producto.** `idf.py -p COM6 flash`; `serial_capture.py --port COM6 --seconds 40 --send info` → `emparejado con 297`, `mqtt: connected`.
- [ ] **Step 3: Prueba real** (ThingsBoard sin desplegar, así que va por el contador). La 297 tiene bebé dentro y sin nombre real → al arrancar sale «Ha llegado un bebé». Pablo prueba en la pantalla: AHORA NO (no vuelve a salir tras `reboot`), tocar «Ponle nombre», escribir «Lucía» → aparece sobre el bebé y sigue tras `reboot`. En el log solo `nombre local: si`.
- [ ] **Step 4: Demo.** Mantener BOOT 2 s: no pregunta y no sale «Ponle nombre».
- [ ] **Step 5:** Commit de lo que haya cambiado (si nada, no hay commit). Preguntar a Pablo si se despliega la Task 1 en ThingsBoard (`tb/tb_apply_incutwin_state.py`).
