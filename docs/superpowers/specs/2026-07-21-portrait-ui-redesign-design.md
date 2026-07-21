# Rediseño UI vertical + vista "incubadora vacía" — Diseño

Fecha: 2026-07-21 · Estado: aprobado por Pablo

## Problema

1. Sin conexión con la IncuNest, el bebé "sleeping" parpadea (aparece y
   desaparece). Se quiere una vista estable: la imagen `IncuNest_empty`.
2. El panel se va a usar en vertical (240×320, USB arriba). Hay que girar
   display y táctil, quitar el chip "Conexión", simplificar el estado a
   solo iconos (temperatura, luz, corazón) y añadir arriba un icono WiFi
   con nivel de cobertura (estilo Windows) y el engranaje de ajustes.

## Decisiones (con el usuario)

- Imagen `IncuNest_empty`: **rotada 90°, a pantalla completa** (240×320).
- Con conexión: se mantiene el **bebé animado** actual con halo.
- Giro: **USB arriba** (si sale invertido, se cambia `offset_rotation`).
- Iconos de estado: **solo icono + color**, sin texto.

## Diseño

### Vista sin conexión

- Nuevo asset `img_incunest_empty` (240×320, RGB565) generado en
  `tools/gen_extra.py` desde `Images/IncuNest_empty.png` (rotado 90° y
  escalado/recortado para llenar la pantalla).
- En `ui_apply_state()`: `!online` → ocultar bebé + halo + mood y mostrar
  la imagen; `online` → lo contrario. La barra superior queda encima.

### Layout vertical del home (240×320)

- **Barra superior**: icono WiFi (izquierda) con 4 niveles por RSSI
  (punto + 0..3 arcos), gris tachado sin WiFi, teñido ámbar si hay WiFi
  pero no nube; engranaje (derecha), conserva click corto → Ajustes y
  pulsación larga → factory reset.
- **Centro**: bebé animado + halo, centrado horizontalmente.
- **Abajo**: fila de 3 círculos con icono (termómetro, lámpara, corazón);
  el color de borde/fondo del círculo indica el estado (gris = sin datos u
  off, verde = estable, ámbar = calentando, rojo = alarma, azul =
  fototerapia, coral = corazón latiendo).
- Se eliminan los 4 chips con texto (incluido "Conexión").

### Giro de pantalla

- `pins_config.h`: `LCD_H_RES 240`, `LCD_V_RES 320`.
- `LGFX_CrowPanel28.h`: `offset_rotation` del panel y del táctil a
  vertical; ajuste del mapeo en `touchpad_read_cb` de `main.cpp`.
  Verificación en hardware; invertir es un cambio de una línea.

### Pantallas afectadas

- **Ajustes**: reorganizar a 240 px de ancho (botones ES/EN más
  estrechos, swatches más pequeños, pie en varias líneas).
- **Onboarding**: elementos centrados; ajustar anchos/QRs que desborden.
- **Splash**: centrado, sin cambios.

### Assets nuevos (gen_extra.py)

- `img_incunest_empty` 240×320 sin alpha.
- `img_wifi_0..img_wifi_3` (niveles) + `img_wifi_off`, ~24×24, navy;
  el tintado (ámbar/gris) se hace con recolor de LVGL.

## Iteración 2 (tras probar en hardware, 2026-07-21)

- Display girado 180° (`offset_rotation` 0 → 2) con el táctil compensado
  en `main.cpp`.
- `IncuNest_empty` pasa a orientación natural (derecha, sin rotar), con
  bandas blancas arriba/abajo.
- **Barra de estado inferior siempre visible** con el estado de IncuTwin
  en palabras y color: Sin conexión WiFi (rojo) / Conectando a la
  nube... (ámbar) / Sin IncuNest vinculada (gris; el nodo de Firebase no
  existe) / IncuNest apagada (gris) / IncuNest sin bebé (azul; campo
  opcional `baby` del esquema, `true` por defecto) / Bebé
  durmiendo·despierto (verde) / ¡Alarma! (rojo). Sustituye a la etiqueta
  de ánimo bajo el bebé.
- **Botón "Agarra mi mano"** (coral) junto a los 3 iconos de estado, solo
  visible cuando hay bebé; misma interacción que tocar al bebé.
- Fila inferior: iconos Ø44 a la izquierda + botón a la derecha; barra de
  estado de 224×26 debajo.

## Verificación

Sin entorno de test host para LVGL/hardware: verificación manual en el
panel — giro y táctil correctos, transición online↔offline sin parpadeo,
niveles WiFi al alejar el router, ajustes y onboarding sin desbordes.
