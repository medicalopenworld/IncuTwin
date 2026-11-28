# «Tu camino» de niveles — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Al tocar la tarjeta del nivel en Corazones se abre «Tu camino» con los cinco niveles, su estado y lo que da cada uno.

**Architecture:** El estado de cada fila (conseguido / siguiente / pendiente, «Próximamente», corazones que faltan) sale de una función pura en `rewards_logic` probada en `test_apps`; `ui_hearts.c` solo pinta las filas en un diálogo con el estilo de bolsillo.

**Tech Stack:** ESP-IDF 6.0.1, C, LVGL 9, Unity on-target.

**Spec:** `docs/superpowers/specs/2026-11-27-camino-de-niveles-design.md`

## Global Constraints

- Rama `feat/afecto-camino` en el worktree `C:\Users\Pablo\Documents\IncuTwin-afecto`, sacada de `origin/feat/acompanar` en fd29f33. Firmware en `IncuTwin_FW/`.
- Compilar con `. C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1`. COM12 se puede flashear (permiso de Pablo); si está ocupado, esperar y reintentar, nunca cerrar procesos. Dejar la placa con el firmware normal.
- Textos exactamente los de la spec (ES/EN). Nunca rojo ni alarma. No cambiar umbrales, nombres de nivel, guardado (`rw_state_t`) ni la tarjeta de subida de nivel.
- «Próximamente» para los niveles por encima de `RW_LEVEL_LAST_OPEN`.
- 240 px de ancho: texto partido en líneas, nunca puntos suspensivos; si no cabe, se desplaza.
- Commit en español con el estilo del repo, terminando en `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Sin push.

## Review Focus

1. Nivel máximo conseguido (150+): no hay fila «siguiente» ni «Te faltan». → test.
2. 0 corazones: Manita conseguida, Caricia siguiente con «Te faltan 10». → test.
3. Siguiente nivel por encima de `RW_LEVEL_LAST_OPEN` (p. ej. 40 corazones): Abrazo es el siguiente, con «Te faltan 35» y «Próximamente» a la vez. → test.
4. Demo: el camino usa el nivel de la demo, no el real. → comprobación en placa.
5. Abrir y cerrar el camino varias veces seguidas no deja objetos ni bloquea la pantalla. → comprobación en placa.

---

### Task 1: «Tu camino»

**Files:**
- Modify: `components/rewards/include/rewards_logic.h`, `components/rewards/rewards_logic.c`
- Modify: `components/ui/ui_hearts.c` (`level_clicked`), `components/ui/ui_i18n.h`, `components/ui/ui_i18n.c`
- Test: `test_apps/main/test_rewards.c`

**Interfaces:**
- Produces:

```c
typedef enum { RW_STEP_DONE = 0, RW_STEP_NEXT, RW_STEP_LATER } rw_step_t;
/* Estado de un nivel en el camino para `earned` corazones ganados en total */
rw_step_t rw_path_step(uint32_t earned, rw_level_t level);
/* true si el nivel aun no abre nada en esta version (por encima de RW_LEVEL_LAST_OPEN) */
bool rw_path_soon(rw_level_t level);
/* Corazones que faltan para `level` (0 si ya esta conseguido) */
uint32_t rw_path_missing(uint32_t earned, rw_level_t level);
```

- Textos nuevos (en el bloque de recompensas de `ui_i18n`): `STR_PATH_TITLE`, `STR_PATH_MISSING` («Te faltan %u corazones»), `STR_PATH_SOON`, `STR_PATH_HAND`, `STR_PATH_CARESS`, `STR_PATH_TICKLES`, `STR_PATH_HUG`, `STR_PATH_SQUEEZE`, con los textos de la spec. Quitar `STR_AFF_HOW_*` si dejan de usarse.

- [ ] **Step 1: Tests**

```c
TEST_CASE("rewards: camino con 0 corazones", "[rewards]") {
    TEST_ASSERT_EQUAL(RW_STEP_DONE, rw_path_step(0, RW_LEVEL_HAND));
    TEST_ASSERT_EQUAL(RW_STEP_NEXT, rw_path_step(0, RW_LEVEL_CARESS));
    TEST_ASSERT_EQUAL(RW_STEP_LATER, rw_path_step(0, RW_LEVEL_TICKLES));
    TEST_ASSERT_EQUAL_UINT32(10, rw_path_missing(0, RW_LEVEL_CARESS));
    TEST_ASSERT_EQUAL_UINT32(0, rw_path_missing(0, RW_LEVEL_HAND));
}

TEST_CASE("rewards: camino con el siguiente aun por llegar", "[rewards]") {
    TEST_ASSERT_EQUAL(RW_STEP_DONE, rw_path_step(40, RW_LEVEL_TICKLES));
    TEST_ASSERT_EQUAL(RW_STEP_NEXT, rw_path_step(40, RW_LEVEL_HUG));
    TEST_ASSERT_EQUAL(RW_STEP_LATER, rw_path_step(40, RW_LEVEL_SQUEEZE));
    TEST_ASSERT_EQUAL_UINT32(35, rw_path_missing(40, RW_LEVEL_HUG));
    TEST_ASSERT_TRUE(rw_path_soon(RW_LEVEL_HUG));
    TEST_ASSERT_TRUE(rw_path_soon(RW_LEVEL_SQUEEZE));
    TEST_ASSERT_FALSE(rw_path_soon(RW_LEVEL_TICKLES));
}

TEST_CASE("rewards: camino completo no tiene siguiente", "[rewards]") {
    for (int l = 0; l < RW_LEVEL_COUNT; l++) {
        TEST_ASSERT_EQUAL(RW_STEP_DONE, rw_path_step(150, (rw_level_t)l));
        TEST_ASSERT_EQUAL_UINT32(0, rw_path_missing(150, (rw_level_t)l));
    }
}
```

- [ ] **Step 2: Compilar `test_apps` y ver que falla** (`rw_path_step` no existe).
- [ ] **Step 3: Implementar las tres funciones** en `rewards_logic.c` (con `rw_level` y `rw_level_min`).
- [ ] **Step 4: Pasar `test_apps` en COM12** con `python tools/serial_capture.py --port COM12 --seconds 25 --reset`: 0 fallos (último total 220 + 3).
- [ ] **Step 5: El diálogo en `level_clicked`.** Sustituye al diálogo actual. Título `STR_PATH_TITLE`, cerrar con la cruz o el botón como los demás diálogos de la pantalla (mira `ui_msgbox` en `ui.c` y reutilízalo si admite contenido propio; si no, el mismo patrón de caja `toy_box` sobre la capa superior que usa la tarjeta de nivel en `ui_notice.c`). Una fila por nivel: marca de estado (conseguido, siguiente, pendiente; dibujada con el corazón de píxeles `toy_heart_img`: lleno, de hoy y vacío), nombre con sus corazones, el texto de lo que da y, debajo, «Te faltan N corazones» en el siguiente y «Próximamente» en los que `rw_path_soon`. Colores del tema (`COL_INK`, `COL_MUTED`, `COL_CORAL`); el nivel del usuario lo da `ui_rewards()` (el de la demo con la demo activa).
- [ ] **Step 6: Placa.** Flashear el firmware normal y, con `pantalla` + `python tools/screenshot.py`, capturar en `C:\Users\Pablo\Documents\IncuTwin\afecto-borrador\capturas\camino\`: demo con `rewards nivel 0`, `1`, `2`, `4` y el camino abierto en cada uno (abrirlo: `ir corazones` y, si no hay forma de tocar la tarjeta desde la consola, añadir a `ir` un destino `camino` que lo abra, como los otros destinos de capturas). Abrir y cerrar varias veces. Mirar cada captura.
- [ ] **Step 7: Commit** `feat(hmi): «Tu camino» con los niveles de afecto y lo que da cada uno`.
