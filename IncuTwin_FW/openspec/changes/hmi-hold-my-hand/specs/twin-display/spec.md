## REMOVED Requirements

### Requirement: Agarra mi mano [manual]
**Reason**: sustituido por el gesto sostenido de un minuto con aviso a la IncuNest (capability
`hold-my-hand` y requisito "Coge mi mano en pantalla").
**Migration**: el botón pasa a llamarse "Coge mi mano"; tocar al bebé ya no lo despierta, sino
que empieza el gesto.

## MODIFIED Requirements

### Requirement: Tres vistas del centro de la pantalla [manual]
La zona central SHALL mostrar exactamente una de tres vistas:

- **BEBÉ** cuando `show_baby`: bebé animado con halo, fila de tres iconos de estado y botón
  "Coge mi mano".
- **CON SUS PAPÁS** cuando `parents_view`: imagen a pantalla completa del bebé con sus padres.
  Con un tono de piel oscuro (`skin` 3..4) SHALL usarse la familia de piel oscura
  (`Baby_with_parents_black`); con 0..2, la de siempre (`Baby_with_parents`).
- **VACÍA** en cualquier otro caso: imagen a pantalla completa de la incubadora vacía.

La barra superior (icono WiFi, marca IncuTwin, engranaje) y la barra de estado inferior SHALL
permanecer visibles y por encima en las tres vistas.

#### Scenario: Bebé dentro
- **WHEN** `link_ok` y el estado dice `baby = in`
- **THEN** se ve el bebé animado, los tres iconos y el botón, y la imagen de incubadora vacía no
  se ve

#### Scenario: Incubadora libre
- **WHEN** `link_ok` y el estado dice `baby = none`
- **THEN** se ve la incubadora vacía a pantalla completa y ni el bebé ni los iconos ni el botón

#### Scenario: Sin enlace
- **WHEN** falla cualquier condición de `link_ok` (sin WiFi, sin broker ≥ 600 s, sin emparejar,
  sin estado recibido, incubadora `offline`)
- **THEN** se ve la vista VACÍA

#### Scenario: Con sus papás
- **WHEN** `link_ok` y `baby = parents` (o `home`)
- **THEN** se ve la imagen del bebé con sus padres a pantalla completa

#### Scenario: Con sus papás, piel oscura
- **WHEN** `link_ok`, `baby = parents` y `skin = 4`
- **THEN** se ve la imagen de la familia de piel oscura

## ADDED Requirements

### Requirement: Coge mi mano en pantalla [manual]
En la vista BEBÉ, el gesto (ver `hold-my-hand`) SHALL presentarse así:

| Estado | Bebé | Fila inferior (sustituye a iconos y botón) | Barra de estado |
|---|---|---|---|
| reposo | estado real | iconos + botón "Coge mi mano" / "Hold my hand" | la normal |
| listo | ojos abiertos; diana coral pulsando sobre el pecho | "Pon tu dedo sobre el bebé" / "Put your finger on the baby" | "Te está esperando" / "Waiting for you" (coral) |
| sosteniendo | ojos cerrados; anillo coral que se llena en `T_min`; corazones que suben | "Le estás dando la mano..." hasta la mitad, luego "El bebé está recibiendo calorcito" / "You're holding their hand...", "The baby is getting your warmth" | "Mantén pulsado para estar mano a mano" / "Keep pressing to stay hand in hand" (coral) |
| gracia | ojos abiertos; anillo atenuado y quieto | "¿Sigues ahí? Vuelve a ponerlo" / "Still there? Put it back" (coral) | "Te espera N s más" / "Waiting N s more", N = segundos que quedan (coral) |
| completado | ojos cerrados; el anillo sigue dando vueltas (ver abajo); fondo coral suave `0xFBEDEA`; corazones | "Gracias por tu calorcito" / "Thank you for your warmth" | "Quédate todo lo que quieras" / "Stay as long as you like" (verde) |
| agradecimiento | imagen a pantalla completa de los papás con la IncuNest y el corazón (`Corazon_IncuNest`), que aparece fundiéndose (≈ 450 ms); pulsable como el bebé | "Gracias por tu calorcito" en caja blanca con borde coral | "Quédate todo lo que quieras" (verde) |

El anillo SHALL rodear el halo (diámetro ≈ 196 px, grosor 6 px, fondo `0xF5C4B3`, relleno
`COL_CORAL`) y avanzar de forma continua (refresco ≤ 200 ms). Al completar una vuelta (`T_min` de
dedo) y seguir con el dedo, SHALL empezar otra vuelta encima de la anterior, que queda completa
como fondo, en el color siguiente de la serie coral `0xDE6F63` → rosa `0xD94E7A` → ámbar
`0xF5A93B` → lila `0x8E6CC9` (y vuelta a empezar). Nunca se muestra un número de segundos salvo
en la barra de estado durante la gracia.

Los textos SHALL caber: la fila admite dos líneas centradas en vertical; si un texto de la barra
de estado no cabe en una línea con la fuente de 14 px, SHALL usarse la de 12 px.

#### Scenario: Del botón al agradecimiento
- **WHEN** con el bebé durmiendo se pulsa "Coge mi mano" y luego se mantiene el dedo 60 s
- **THEN** al pulsar el botón el bebé abre los ojos y aparece la diana; al tocarle cierra los ojos
  y el anillo empieza a llenarse; a los 60 s el fondo se vuelve coral suave y aparece "Gracias por
  tu calorcito"

#### Scenario: Soltar un momento
- **WHEN** en sosteniendo se levanta el dedo 3 s
- **THEN** el bebé abre los ojos, el anillo se queda quieto, la barra dice "Te espera 7 s más" y al
  volver el dedo el anillo sigue desde donde estaba

#### Scenario: Más de una vuelta
- **WHEN** tras completar se mantiene el dedo 30 s más
- **THEN** el anillo coral queda completo y encima avanza una segunda vuelta rosa hasta la mitad

#### Scenario: Fin del gesto
- **WHEN** en completado se levanta el dedo
- **THEN** aparece la imagen de los papás con la IncuNest y el corazón durante 4 s; después
  vuelven el bebé, los iconos, el botón, el estado real de los ojos y la barra de estado normal

#### Scenario: Volver durante el agradecimiento
- **WHEN** con la imagen final en pantalla se vuelve a poner el dedo
- **THEN** vuelve el bebé con los ojos cerrados y el anillo sigue desde donde estaba

### Requirement: Etiqueta con el nombre [manual]
En la vista BEBÉ, si el estado trae `name` no vacío, SHALL verse una etiqueta blanca con borde
coral y el nombre en negrita, centrada sobre la parte baja de la manta (por encima de la fila de
iconos), en todos los estados del gesto salvo el agradecimiento (imagen final). Sin `name` la etiqueta NO SHALL verse (no se pone texto
de sustitución). Un nombre largo SHALL truncarse con puntos suspensivos dentro de 200 px.

#### Scenario: Con nombre
- **WHEN** `show_baby` con `name = "Lucía"`
- **THEN** se ve la etiqueta "Lucía" sobre la manta

#### Scenario: Sin nombre
- **WHEN** `show_baby` y el estado no trae `name`
- **THEN** no se ve ninguna etiqueta

### Requirement: Etiqueta del país [manual]
Si el estado trae `country`, en las vistas BEBÉ y CON SUS PAPÁS SHALL verse arriba a la izquierda
(bajo el icono WiFi) una etiqueta pequeña blanca con borde azul, el icono de ubicación y el nombre
del país en el idioma del panel ("Camerún" / "Cameroon"). Un código que el panel no conozca SHALL
mostrarse tal cual. Sin `country`, o durante el agradecimiento del gesto, NO SHALL verse.

#### Scenario: Camerún
- **WHEN** `show_baby` con `country = "CM"` y el panel en español
- **THEN** se ve la etiqueta "Camerún" arriba a la izquierda

#### Scenario: Código desconocido
- **WHEN** `show_baby` con `country = "XK"`
- **THEN** la etiqueta dice "XK"

### Requirement: Páginas de la demo [manual]
Con la demo activa, la pantalla principal SHALL tener tres páginas que se cambian deslizando el
dedo a izquierda (siguiente) o derecha (anterior), con una animación de deslizamiento (≈ 260 ms) y
tres puntos de página sobre la barra de estado (el actual, una píldora coral):

| Página | Contenido (datos simulados) |
|---|---|
| 1 | la de siempre (bebé o la vista que toque) |
| 2 | bandera y nombre del país (Camerún); tarjeta "Hospital" con "Hospital de Yaundé"; tarjeta "Hora local" con la hora de Camerún (UTC+1, de la hora SNTP; "--:--" sin hora) |
| 3 | "Logros" / "Achievements", "Lo que hemos logrado juntos", y cuatro tarjetas: manos dadas (37 + los gestos completados en esta demo), 12 familias le acompañan, 14 días en la IncuNest, +420 g de peso ganado |

Las barras superior y de estado SHALL seguir visibles en las tres páginas. Fuera de la página 1 el
gesto "Coge mi mano" vuelve a reposo y los toques no llegan al bebé. Con un gesto completado (o
agradeciendo) no se cambia de página. Fuera de demo no hay páginas ni puntos y deslizar no hace
nada.

#### Scenario: Ver el país
- **WHEN** en demo se desliza el dedo hacia la izquierda sobre la pantalla principal
- **THEN** entra la página 2 con la bandera de Camerún, el hospital y la hora local

#### Scenario: Logros que suben
- **WHEN** en demo se completa un "Coge mi mano" y se desliza dos veces a la izquierda
- **THEN** la tarjeta de manos dadas dice 38

#### Scenario: Fuera de demo
- **WHEN** sin demo se desliza el dedo
- **THEN** no cambia nada

### Requirement: Latido al tocar el corazón [manual]
El icono del corazón de la fila de estado SHALL ser pulsable: con `bpm > 0` pone a sonar el latido
al ritmo real del bebé (ver `sound`, "Latido audible") y se rellena de coral suave mientras suena;
otro toque lo para. Con `bpm = 0` no hace nada.

#### Scenario: Oír al bebé
- **WHEN** se toca el corazón con `bpm = 140`
- **THEN** suena el latido a 140 lpm durante 10 s y el icono se ve iluminado
