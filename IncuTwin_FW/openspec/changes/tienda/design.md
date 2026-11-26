# tienda — Diseño

## Context

- `rewards` guarda un blob versionado (`RW_VERSION 1`) en NVS `rewards/state`; si la versión no
  cuadra, `rewards_init` lo reinicia. `unlocked` es de 16 bits y ya usa 11.
- El bebé son 8 fotogramas I8 (niño y niña × dormido, bostezo 1 y 2, despierto) con una paleta
  común de 95 colores; el tono de piel se cambia copiando una de las 5 paletas de tono al principio
  de cada fotograma (copias en PSRAM). Gorro y manta comparten hoy los mismos azules (y rosas).
- La tienda vive al final de la lista de Corazones (`ui_hearts.c`) como filas.

## Decisions

### 1. Guardado v2 en una clave nueva, con migración desde v1
`rewards/state2` con `RW_VERSION 2`. Al arrancar: si hay `state2` válido se usa; si no, se lee
`state` con el tamaño de v1 y, si es v1, se migra campo a campo (los bits de `unlocked` de v1 valen
igual en v2 porque los artículos nuevos van detrás). `state` no se borra: si el OTA hace rollback,
el firmware anterior encuentra sus corazones (con lo de antes de actualizar).
- *Descartada*: subir la versión en la misma clave. `rewards_init` reiniciaría a todo el mundo.
- *Descartada*: reservar bits en v1 sin migrar. No caben ni los de la fase 1.

`unlocked` pasa a 64 bits para que la fase 2 (Osito, Conejita, fondos y efectos nuevos) no necesite
otra migración.

### 2. Papá Noel y los copos como interruptores, no como elección
`santa_on` y el efecto Copos se ponen y se quitan como la guirnalda. Fuera de temporada el bebé
lleva el gorro elegido y no hay copos, sin tocar lo elegido.
- *Descartada*: Papá Noel como un gorro más de niño y de niña. Habría que recordar el anterior para
  volver a él el 7 de enero.

### 3. La temporada la decide la lógica pura con la fecha local
`rw_xmas_season(month, mday)`: del 24-dic al 6-ene, ambos incluidos. `rw_unlock` recibe la fecha
(día del mes y mes, 0 si no hay hora) y devuelve el motivo si no se puede: sin saldo, fuera de
temporada, no se compra (guirnalda sin conseguir) o ya canjeado. Sin hora no se compra nada de
Navidad.

### 4. Gorro y manta por paleta, con entradas propias
`gen_assets.py` separa, después de cuantizar, los píxeles recoloreables (azules del niño, rosas de
la niña, con brillo) del gorro y de la manta en entradas de paleta propias (copias). Para cada color
de la tienda genera una tabla con los colores de esas entradas (`baby_hat_colors`,
`baby_blanket_colors`). En tiempo de ejecución `baby_widget` compone: paleta de tono → colores del
gorro → colores de la manta, en las copias de PSRAM. Cero bytes de imagen nuevos.
- *Descartada*: fotogramas por color. 8 × 22 KB por cada uno.
- *Descartada*: recolorear en HSV en el panel. Más código y colores distintos de las maquetas.

La máscara del gorro sale de rellenar desde dentro del gorro en `Images/Baby.png` con el contorno
marino como muro (así no se tiñe la capucha de la manta).

### 5. Papá Noel es un sprite encima de la cabeza
ARGB8888 pequeño (dibujado en píxeles al tamaño de celda del bebé), hijo de la imagen del bebé para
que respire con ella (`LV_OBJ_FLAG_OVERFLOW_VISIBLE` porque sobresale por arriba y por la derecha).
Tapa el pompón y el lazo.

### 6. Tienda en su propia pantalla
`ui_shop.c`: título con «‹» y saldo, pestañas, conmutador Niño/Niña en Gorros y rejilla de
tarjetas de 3 columnas con scroll. Probar es una segunda vista de la misma pantalla con una copia
propia de un fotograma (22 KB de PSRAM mientras se ve) para no cambiar el bebé de Inicio. Halo,
fondo, guirnalda y el canje siguen el flujo de diálogo de siempre.
- *Descartada*: seguir con filas en la lista de Corazones. Con 30 artículos no se encuentra nada.

### 6b. Miniaturas reducidas a mano
El pintado de LVGL no escala imágenes con paleta (I8): las miniaturas de gorros y mantas se reducen
en el panel a ARGB8888 promediando cajas de píxeles (`baby_preview_create` con `scale`).

### 6c. Revisar la pantalla desde el PC
`pantalla` vuelca por la consola la pantalla (RGB565) y la capa superior (ARGB8888), comprimidas
(RLE + base64); la captura se pinta en la tarea de LVGL (la de la consola se queda sin pila).
`tienda …` abre cada pestaña, Probar y el diálogo; `demo on|off` y `rewards comprar <id>` (solo en
la demo) permiten ver compras sin gastar corazones reales. `tools/screenshot.py` abre el puerto sin
resetear la placa y guarda PNG (también por lotes).

### 7. Copos con objetos de LVGL
12 copos (etiquetas «*» de 14 px, blancas con contorno suave) que bajan despacio alrededor del bebé
con animaciones de LVGL (10–16 s por caída, desfasadas). Se ocultan mientras un bebé te llama, en el
gesto de «Coge mi mano» y fuera de Inicio.

### Tareas y eventos
Sin tareas nuevas. La compra va por `rewards_unlock` (tarea de LVGL → mutex → NVS) y vuelve como
`TWIN_EVT_REWARDS_CHANGED`; la UI repinta el bebé y la tienda en el refresco de `ui.c`.

## Risks / Trade-offs

- [La migración falla en una IncuTwin de un socio] → test de la migración con un blob v1 real en
  `test_apps`; si algo no cuadra se cae a `rw_init` como hoy (mismo riesgo que antes, no mayor).
- [Bordes del gorro con colores mezclados] → solo se recolorean entradas con brillo alto; los bordes
  oscuros se quedan como el contorno.
- [La fecha del panel sin hora] → nada de Navidad se compra ni se ve sin hora.

## Migration Plan

Firmware nuevo por OTA: la primera vez migra `state` → `state2` y guarda. Rollback: el firmware
anterior sigue leyendo `state`.
