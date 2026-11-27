# Estilo «IncuTwin de bolsillo» en la pantalla del aparato

Fecha: 27-nov-2026. Rama `feat/hmi-estilo-bolsillo` (desde `feat/acompanar`, firmware 2.0.3).

## Por qué

Pablo ha aprobado para la app móvil el estilo «IncuTwin de bolsillo» (guía: `incutwin-app/docs/diseno-app.md`)
y quiere que el aparato tenga **el mismo diseño y la misma interfaz**, para que aparato y app se reconozcan como la
misma cosa. La colocación ya es la del aparato (la app la copió); lo que cambia es el aspecto.

## Objetivos

- Colores de la app: fondo `#F7EEF3`, tarjetas `#FFFDF8`, tinta `#1F2A5A`, texto secundario `#5E6280`, apagado
  `#B9B3C6`, coral `#DE6F63` / `#FBE3DF` / `#B04E43`, pestaña activa `#F1E2EA`, pantallita `#EAF2FB`.
- Letra de la app: **Fredoka 600** en títulos, botones, nombres y números; **Nunito** en el resto (sustituye a
  Montserrat, con los mismos símbolos de FontAwesome).
- Piezas de juguete: borde 2 px tinta, sombra dura sin difuminar (2 px piezas pequeñas, 3 px tarjetas y botones),
  esquinas 10–12; al pulsar baja 2 px hacia su sombra.
- **Inicio con pantallita (propuesta B, aprobada)**: el bebé, su halo, el fondo comprado y el gesto dentro de un marco
  tinta con sombra dura. Pablo la quiere **más rectangular** que la maqueta: esquinas pequeñas (8 px).
- **Corazones de píxeles** (cuadrícula 9 × 8 de la app) en saldo, precios, nivel y racha. **Se mantiene el icono de
  la IncuNest** en la tarjeta «Una IncuNest con tu nombre».
- Barra de abajo: fondo crema, raya tinta arriba, pastilla `#F1E2EA` en la pestaña activa, iconos tinta.
- Tienda: revisar que los iconos (gorros, mantas, fondos, efectos) se vean bien en las fichas nuevas.
- Fondo de serie de la tienda: «Azul bebé / Baby blue» `#EAF2FB` (igual que la app), pintado dentro de la pantallita.

## No objetivos / lo que no se toca

- Los dibujos (bebé, IncuNest, papás, agradecimiento, mapa): no se rehacen ni se reescalan.
- Lo que hace cada pantalla y cada botón, textos (salvo el nombre del fondo), animaciones.
- Trama de puntitos de la pantallita de la app (a 240 px parecería suciedad).

## Fracaso aunque funcione

Algo que parezca una alarma (nada rojo: los dos `COL_RED` pasan a coral), el bebé o el anillo del gesto recortados
de forma fea, letra que no se lea en 240 × 320, o la pantalla más lenta.

## Restricciones

- Programa: 3,48 MB de 4 MB por ranura OTA. Nunito sustituye a Montserrat (mismo tamaño); Fredoka añade ~50 KB.
- LVGL 9.6 no dibuja sombras duras (la sombra con difuminado 0 no se pinta): se dibujan con un evento
  `DRAW_MAIN_BEGIN` y `REFR_EXT_DRAW_SIZE`, como la vista de detrás de la app.
- 240 × 320: la pantallita va de x 6 a 232 y de y 35 a 228; la fila de iconos y botón baja a y 235.

## Enfoques considerados

| | Enfoque | Por qué sí / no |
|---|---|---|
| **1** | Utilidades `ui_toy.c` (caja, botón, etiqueta, corazón de píxeles, sombra dura) y pasar cada pantalla a ellas | **Elegido.** Mismo patrón que la app (`components/toy`), cambios locales por pantalla |
| 2 | Tema LVGL propio que restile todo de una vez | Toca a la vez todas las pantallas y choca con los estilos sueltos que ya pone cada una |
| 3 | Solo cambiar colores y letra | No da el aspecto de juguete que pidió Pablo |

## Decisiones de quien diseña (Pablo puede cambiarlas)

- La pantallita sale con el bebé (reposo, gesto, sin WiFi) y con las páginas del mapa y la racha. Los dibujos a
  pantalla completa (IncuNest vacía, con sus papás, agradecimiento) siguen a pantalla completa como hoy, porque no caben
  en el marco sin reescalarlos.
- Mismas medidas que la app en píxeles (la pantalla del aparato tiene una densidad parecida a la de un móvil).
- Títulos de pantalla centrados en Fredoka 20.

## Cómo se comprueba

Pablo permitió flashear la IncuTwin del COM12 durante el trabajo (al terminar vuelve a la 2.0.3). Capturas con
`tools/screenshot.py` antes y después de cada pantalla; compila con `idf.py build`; tests de lógica sin cambios.

## Resultado (27-nov-2026)

Capturas de la IncuTwin del banco (DEEF3C, COM12) en `docs/diseno/antes/` (2.0.3) y `docs/diseno/despues/`.

- Piezas en `components/ui/ui_toy.c` (`toy_box`, `toy_button`, `toy_pill`, `toy_segment`, `toy_shadow`,
  corazones de píxeles `toy_heart_*`, hechos en tiempo de ejecución en PSRAM). Letras regeneradas con
  `tools/gen_fonts.py` (Nunito Bold en `lv_font_es_*`, Fredoka SemiBold en `lv_font_title_12..24`).
- Programa: 3,56 MB de 4 MB (antes 3,48 MB).
- Pasadas: Inicio (pantallita, fila, etiquetas), gesto, mapa, racha, Notificaciones, Corazones, Tienda (fichas,
  Probar dentro de su pantallita, gorro de Papá Noel bien colocado en la ficha), Historial, Ajustes, diálogos,
  franja de avisos, teclado del nombre, puesta en marcha y actualización.
- Sin tocar: IncuPray (desactivada), la pantalla de arranque y los dibujos.
- Para revisar pantallas por la consola: `ir <inicio|avisos|corazones|historial|ajustes|bebe|mapa|racha|listo|dedo|soltar|fin>`
  y `tools/screenshot.py`.
- Los nombres largos de las fichas de la tienda («Verde agua», «Desde el 24») pasan despacio, como antes.
