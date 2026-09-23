# Estado "Baby with parents" — diseño

**Fecha:** 2026-07-21
**Estado:** aprobado (diseño validado en conversación)
**Módulos:** `src/ui/ui_main.cpp`, `src/ui/i18n.*`, `src/sound/`,
`src/assets/`, `tools/gen_extra.py`, `include/config.h`

## Objetivo

Cuando el bebé deja de mostrarse en pantalla (transición de "bebé dentro"
a cualquier otro estado), celebrar el momento: imagen a pantalla completa
`Baby_with_parents` y melodía festiva. El estado dura **15 minutos** salvo
que el bebé vuelva a mostrarse antes; al expirar, IncuTwin vuelve al
estado que corresponda (incubadora vacía, offline, etc.).

## Contexto

- `ui_apply_state()` (`src/ui/ui_main.cpp`) deriva
  `show_baby = online && st.baby_present` y es el único punto que conoce
  la transición. `online` incluye WiFi + nube + nodo + IncuNest + datos
  frescos (`DATA_STALE_S`).
- La vista "incubadora vacía" ya usa una imagen full-screen RGB565
  (`img_incunest_empty`, generada por `tools/gen_extra.py`).
- El buzzer 8-bit (`src/sound/`) reproduce secuencias `{freq, dur}` no
  bloqueantes desde un `lv_timer`, con toggle persistido en NVS.
- `Images/Baby_with_parents.png` (896×1199 RGBA) existe pero no está
  convertida a asset.

## Decisión de disparador (elección del usuario)

Se activa en **cualquier flanco de bajada de `show_baby`**, no solo con
`baby_present=false` explícito. Implicación asumida: una pérdida de
WiFi/nube o datos caducos con el bebé dentro también muestra "con sus
papás" durante la ventana; al expirar se pinta la vista offline normal.

## Enfoque: override en la capa UI

Descartados: estado derivado en `app_state` (la condición depende del
staleness que vive en la UI; mezclaría estado cosmético temporal con el
estado real de red) y módulo aparte con su propio timer (sobredimensionado).

### Asset (`tools/gen_extra.py`, `src/assets/`)

- Nueva función calcada de `make_incunest_empty()`: recorta márgenes
  blancos de `Images/Baby_with_parents.png` y encaja en 240×320 con
  fondo blanco (`ImageOps.pad`).
- Se emite `img_baby_parents` con `emit_true_color` (RGB565 sin alfa,
  ~150 KB de flash) en `img_extra.c` + extern en `assets.h`.
- Preview en `tools/preview/baby_parents.png`.

### UI (`src/ui/ui_main.cpp`)

- Nueva imagen full-screen `s_parents_img`, oculta por defecto, creada
  junto a `s_empty_img` (fondo del z-order; la barra superior queda
  encima).
- Estado del modo: `static uint32_t s_parents_until = 0` (0 = inactivo)
  y `static bool s_prev_show = false` para el flanco. Comparaciones de
  tiempo con resta (`(int32_t)(millis() - s_parents_until) < 0`) para
  tolerar rollover.
- En `ui_apply_state()`:
  - Flanco `show_baby` true→false: `s_parents_until = millis() +
    PARENTS_MODE_MS` y `sound_play_parents()`.
  - Flanco false→true: `s_parents_until = 0` (cancelación inmediata).
  - `parents_active = !show_baby && ventana vigente`.
- Render con `parents_active`:
  - Se muestra `s_parents_img`; se ocultan bebé, iconos, botón de mano
    e `s_empty_img` (extensión de `home_set_view()` a tres vistas).
  - Halo apagado (`HALO_OFFLINE`).
  - Barra de estado: `STR_ST_PARENTS` en `COL_OK`, con prioridad sobre
    los estados no-bebé (pero nunca sobre `show_baby=true`).
- Al expirar la ventana no hay nada que restaurar: el refresh periódico
  (250 ms / 1 s forzado) pinta el estado real.
- Arranque sin bebé: `s_prev_show` nace `false`, no hay flanco ni
  celebración.

### i18n (`src/ui/i18n.*`)

- `STR_ST_PARENTS`: ES "Con sus papás", EN "With parents".

### Sonido (`src/sound/`)

- Nueva `void sound_play_parents(void);` — fanfarria festiva ~3 s en el
  estilo 8-bit existente (arpegio ascendente tipo "level clear"),
  reproducida **una sola vez** al entrar al estado.
- Respeta el gating de sonido vigente (toggle NVS hoy; niveles de
  volumen cuando se implemente esa spec).
- El edge-detect de `sound_on_state()` (fanfarria de bebé detectado +
  latido) no cambia.

### Configuración (`include/config.h`)

- `PARENTS_MODE_MS` = 15 min (`15UL * 60UL * 1000UL`).

## Casos límite

- Flancos repetidos (bebé fuera → dentro → fuera): cada nueva salida
  reinicia la ventana y vuelve a sonar la fanfarria.
- Cambios de estado no-bebé durante la ventana (p. ej. alarma sin
  `baby_present`): la vista "con sus papás" se mantiene hasta expirar;
  si `show_baby` vuelve a true (incluye alarma con bebé), se cancela y
  se pinta el estado real al instante.
- Sonido desactivado: la imagen y la barra cambian igual; solo se omite
  la melodía.
- Cambio de idioma durante la ventana: `update_texts()` fuerza
  `g_state_dirty`; el siguiente `ui_apply_state()` repinta la barra en
  el idioma nuevo sin tocar la ventana.

## Verificación

Sin entorno de test nativo (UI + LEDC): verificación manual con el build
`_sim`:

1. Escenario con bebé → POST `/state {"baby":false}`: imagen de papás,
   melodía una sola vez, barra "Con sus papás" en verde.
2. Reponer `baby=true` antes de 15 min: vuelve el bebé animado al
   instante, sin melodía extra.
3. Dejar expirar la ventana (bajar `PARENTS_MODE_MS` temporalmente para
   la prueba): pasa a incubadora vacía / "Sin bebé".
4. Con bebé dentro, simular caída de conexión: también entra al estado
   (decisión de disparador) y al expirar muestra la vista offline.
5. Toggle de sonido en off: sin melodía, resto igual.
