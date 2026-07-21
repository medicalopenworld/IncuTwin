# Nivel de volumen en ajustes — diseño

**Fecha:** 2026-07-21
**Estado:** aprobado (diseño validado en conversación)
**Módulos:** `src/sound/`, `src/ui/ui_main.cpp`, `src/ui/i18n.*`

## Objetivo

Permitir elegir el nivel de volumen del buzzer desde la página de ajustes,
sustituyendo el switch on/off actual por cuatro niveles discretos:
**Apagado / Bajo / Medio / Alto**.

## Contexto

- El buzzer pasivo se controla por LEDC (`ledcWriteTone`, canal 2, 10 bits)
  en `src/sound/sound.cpp` (`BUZZER_LEDC_CH 2`, pin `BUZZER_PIN`).
- Hoy existe un switch on/off en ajustes persistido en NVS
  (`"incutwin"/"sound"`, bool, default on).
- `ledcWriteTone()` fija ~50 % de duty; el volumen percibido de un buzzer
  pasivo se reduce bajando el duty con `ledcWrite()` tras cada cambio de
  frecuencia.

## UI (`src/ui/ui_main.cpp`)

- En la sección de sonido de ajustes se elimina el switch (`s_sw_sound`) y
  se crea una fila de 4 botones bajo la etiqueta `STR_SOUND`, siguiendo el
  patrón visual y de refresco de los botones de idioma
  (`s_btn_es`/`s_btn_en` + `lang_btns_refresh`).
- El botón del nivel activo se resalta igual que el idioma activo.
- Al pulsar un nivel distinto de Apagado suena un pitido corto de prueba a
  ese volumen, para juzgar la sonoridad en el momento.
- Etiquetas nuevas en i18n: `STR_VOL_OFF`, `STR_VOL_LOW`, `STR_VOL_MID`,
  `STR_VOL_HIGH` (ES: Apagado/Bajo/Medio/Alto; EN: Off/Low/Mid/High).
  Textos cortos: la pantalla es de 240 px de ancho.
- `update_texts()` refresca también estas etiquetas al cambiar de idioma.

## Sonido (`src/sound/sound.h` / `sound.cpp`)

- API nueva (sustituye a `sound_set_enabled`/`sound_is_enabled`):
  - `void sound_set_volume(uint8_t level);` — 0..3 (0 = apagado).
  - `uint8_t sound_get_volume(void);`
  - `void sound_play_test(void);` — pitido corto de prueba para la UI.
- Implementación del volumen: tras cada `ledcWriteTone(ch, freq)` con
  `freq > 0` se aplica `ledcWrite(ch, duty)` con el duty del nivel activo.
- Mapeo inicial (10 bits, percepción logarítmica), a calibrar en hardware:
  - Alto = 512 (50 %, máximo del buzzer)
  - Medio ≈ 60
  - Bajo ≈ 8
  - Apagado = silencio total (no se genera tono).
- Nivel 0 corta cualquier reproducción en curso (mismo comportamiento que
  el off del switch actual: `seq_stop()` + cierre de la ventana de latido).
- Volumen global único para todas las fuentes (jingle de arranque,
  fanfarria de bebé, latido). Sin volúmenes por evento (YAGNI).

## Persistencia (NVS)

- Clave nueva: `"incutwin"/"vol"` (uint8, default 3 = Alto).
- Migración en `sound_init()`: si `"vol"` no existe pero `"sound"` sí,
  `sound=false → vol 0`, `sound=true → vol 3`. La clave vieja deja de
  escribirse (se puede dejar huérfana; no se borra).
- Valores fuera de rango leídos de NVS se saturan a 0..3.

## Casos límite

- Cambio de nivel durante una melodía en curso: el nuevo duty se aplica a
  partir de la siguiente nota (el tick de 15 ms re-tonifica cada nota).
- Nivel 0 durante el latido audible: silencio inmediato.
- NVS corrupta/valor > 3: se satura a 3.

## Verificación

Sin entorno de test nativo (depende de LEDC/LVGL): verificación manual
documentada en hardware:

1. Cada nivel suena con sonoridad claramente distinta (pitido de prueba).
2. Apagado silencia todo, incluido latido y fanfarria.
3. El nivel persiste tras reinicio.
4. Migración: dispositivo con `"sound"=false` y sin `"vol"` arranca en
   Apagado; con `"sound"=true` arranca en Alto.
5. Cambio de idioma refresca las etiquetas de los botones.
