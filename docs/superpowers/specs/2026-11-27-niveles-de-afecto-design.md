# Niveles de afecto: cada nivel abre una forma nueva de tocar al bebé

Fecha: 27-11-2026 · Estado: borrador para aprobar · Base: rama `feat/acompanar` (firmware 2.0.2)
y placa de `feat/acelerometro`

## Para qué

Hoy los niveles (Chispa, Calorcito, Abrazo, Manta, Sol) son solo una etiqueta y una barra en la
pantalla de Corazones: subir no cambia nada. Pablo quiere que subir de nivel se note y que el
premio sea del mismo tipo que lo que se premia, es decir, lo que hace quien acompaña. Por eso los
niveles pasan a llamarse como gestos de cariño y cada uno abre una interacción nueva con el bebé.

## Objetivos

- Cinco niveles con nombres de afecto: **Manita, Caricia, Cosquillitas, Abrazo, Achuchón**
  (decisión de Pablo, 27-nov-2026).
- Cada nivel abre una interacción, y las anteriores se conservan:

| Nivel | Corazones ganados | Interacción que abre | Con qué |
|---|---|---|---|
| Manita | 0 | «Coge mi mano», como hoy | Pantalla |
| Caricia | 10 | Si acaricias al bebé, sonríe | Pantalla |
| Cosquillitas | 30 | Si le acaricias rápido, se ríe | Pantalla |
| Abrazo | 75 | Te apoyas la IncuTwin en el pecho y sientes su latido | Acelerómetro y motor |
| Achuchón | 150 | Abrazada, la meces y el bebé se queda profundamente dormido | Acelerómetro y motor |

- Al subir de nivel, el aviso dice qué se ha abierto y cómo se hace.
- El primer salto llega pronto: con solo el gesto diario (unos 8 corazones a la semana), Caricia
  sale en 9 días, Cosquillitas en 4 semanas, Abrazo en 9 semanas y Achuchón en algo más de 4 meses.

## No objetivos

- Que las interacciones nuevas den corazones, cuenten para la racha o para el súper corazón. Los
  corazones siguen saliendo solo de «Coge mi mano» (y de la IncuPray cuando se lance).
- Mandar nada nuevo a la IncuNest ni a ThingsBoard. Todo ocurre en la IncuTwin.
- Regalos de corazones al subir de nivel (se habló y se deja fuera: la interacción es el premio).
- Niveles por encima de Achuchón.
- Llevarlo a la app del móvil. Queda apuntado para la paridad, no se hace aquí.
- El latido con el motor durante «Coge mi mano» (para eso se puso el motor en la placa). Usa el
  mismo motor, pero es otro cambio.
- Lanzar la IncuPray. Sigue desactivada hasta que Pablo diga.
- Un interruptor para encender o apagar los niveles de afecto. Salen cuando Pablo publique el
  firmware que los lleva, como la tienda.

## Lo que sería un fracaso aunque funcione

- Que agitar o sacudir la IncuTwin provoque cualquier reacción, buena o mala. Nunca se premia
  agitar a un bebé y nunca se regaña: un movimiento brusco no hace nada.
- Que el bebé suelte una carcajada o haga algo que un prematuro no hace. La «risa» es suave.
- Que una caricia se confunda con «Coge mi mano» o le quite progreso a un gesto en marcha.
- Que salga algo en rojo, una alarma o un texto de culpa («hoy no le has abrazado»).
- Que una IncuTwin sin la placa (o con la placa mal enchufada) se cuelgue, se reinicie o enseñe
  un error al usuario.
- Que alguien pierda corazones, sellos o lo comprado al actualizar.

## Opciones (elegida: A, en dos fases)

- **A. Todo en el panel (elegida).** El nivel ya vive en la IncuTwin (`rewards`), así que la
  IncuTwin decide qué interacción está abierta y la reconoce ella sola. No toca el broker ni el
  servidor, y funciona sin red. Se hace en dos fases para no esperar a la placa:
  fase 1, nombres, umbrales, Caricia y Cosquillitas (solo pantalla);
  fase 2, firmware de la placa, Abrazo y Achuchón.
- B. Niveles e interacciones abiertas decididos por el servidor (la API del VPS), para que la
  IncuTwin física y la del móvil compartan nivel. Es a donde habrá que ir cuando la app tenga
  corazones, pero hoy obliga a montar servidor para algo que está desactivado.
- C. Esperar a la placa y sacarlo todo junto. Más simple de explicar, pero retrasa lo táctil
  por una pieza de hardware que aún no se ha medido en una unidad real.

Pablo dejó la forma de construirlo en mis manos («adelante»); A es mi elección.

## Diseño

### Niveles

- `LEVEL_MIN` pasa de `{0, 25, 75, 150, 300}` a `{0, 10, 30, 75, 150}`.
- Nombres ES/EN: Manita/Little hand, Caricia/Caress, Cosquillitas/Tickles, Abrazo/Hug,
  Achuchón/Bear hug.
- El nivel sigue saliendo de los corazones ganados en total; gastar no lo baja.
- No hace falta migrar el guardado: el nivel se calcula, no se guarda. No se toca `RW_VERSION`.
- Los corazones y los niveles ya están en las IncuTwin desde el firmware 2.0.1 (solo la IncuPray
  está apagada), así que al actualizar habrá gente con corazones. Como todos los umbrales nuevos
  son más bajos que los de antes, nadie baja de nivel; algunos suben sin haber ganado nada ese día.
- Para ellos, la primera vez que arranca el firmware nuevo con un nivel por encima de Manita sale
  una sola vez el aviso de lo que tienen abierto. Se recuerda en uno de los bytes reservados del
  guardado (sin migración).
- La barra de Corazones y el aviso «¡Nivel más alto!» siguen igual, con los nombres nuevos.

### Qué está abierto

- Una interacción está abierta si el nivel actual es igual o mayor que el suyo.
- En la demo manda el nivel de la demo, que empieza en Caricia (12 corazones), así que la demo
  enseña la caricia. Para enseñar las demás hará falta un comando de consola que suba el nivel de
  la demo (como `rewards comprar` en la tienda).
- Las interacciones nuevas solo existen en la vista del bebé, con bebé a la vista y con «Coge mi
  mano» en reposo. Durante un gesto de «Coge mi mano» no hay caricias ni abrazos.

### Caricia (pantalla)

- **Gesto:** el dedo se mueve sobre el bebé, sin pulsar antes el botón. Hoy, tocar al bebé en
  reposo enseña la burbuja «Pulsa "Coge mi mano" para acompañarle»; eso sigue igual para un dedo
  quieto. Si el dedo recorre una distancia mínima sobre el bebé, es una caricia y no sale la
  burbuja.
- **Respuesta:** el dibujo de reposo ya sonríe con los ojos cerrados, así que la caricia se ve
  con una sonrisa más marcada, mofletes más sonrosados y un corazón pequeño que sube junto a la
  cabeza, unos segundos, y vuelve a como estaba (aspecto aprobado por Pablo, ver «Dibujos
  nuevos»). Si estaba despierto, cierra los ojos mientras dura (igual que al tocarle en «Coge mi
  mano»).
- **Sin nivel:** en Manita, acariciar no hace nada distinto de hoy.

### Cosquillitas (pantalla)

- **Gesto:** el mismo que la caricia pero rápido: varios cambios de sentido del dedo en poco
  tiempo sobre el bebé.
- **Respuesta:** «risa suave»: el bebé se encoge un poco (un pequeño vaivén del dibujo) con una
  sonrisa más abierta, un par de segundos, y vuelve a la sonrisa de la caricia. Nada de boca
  abierta de carcajada ni de sonido.
- **Sin nivel:** en Caricia, acariciar rápido se queda en la sonrisa de la caricia.

### Abrazo (acelerómetro y motor)

- **Gesto:** con la vista del bebé en pantalla, coges la IncuTwin y la apoyas en el pecho unos
  10 s. La IncuTwin lo reconoce porque ha dejado de estar apoyada en una mesa, está más o menos
  de pie y se mueve muy poco, con el vaivén lento de un cuerpo que respira.
- **Respuesta mientras dura:** el motor da un latido suave que sigue al corazón del bebé: más
  rápido si el suyo va más rápido. No tiene que ser el `bpm` exacto, porque a 120–170 latidos por
  minuto un motor de moneda se queda en un zumbido; se parte de un pulso cada dos latidos y se
  ajusta con una unidad real. Sin `bpm`, un ritmo tranquilo fijo. Es lo único que se nota, porque
  la pantalla queda contra el pecho.
- **Respuesta al mirarla de nuevo:** el bebé con cara tranquila, el halo melocotón y dos
  corazones, y «Gracias por el abrazo» unos segundos.
- **Sin botón:** no hay que pulsar nada antes. Como no da corazones ni manda nada, un abrazo
  reconocido por error (al llevarla en la mano) no hace daño: unos latidos y nada más.

### Achuchón (acelerómetro y motor)

- **Gesto:** durante un abrazo, la meces despacio (un vaivén lento y regular) unos 20 s.
- **Respuesta:** el latido del motor se va espaciando y suavizando, y al mirarla el bebé está
  dormido con la boca en «o», el halo y las «z Z», con «Gracias por el achuchón».
- **Sin nivel:** en Abrazo, mecerla se queda en un abrazo.

### Con el cable puesto

La IncuTwin no lleva batería: va siempre enchufada (Pablo, 27-nov-2026). Abrazo y Achuchón se
hacen con el cable puesto, así que:

- El mecer que se reconoce es corto y suave, de los que caben con un cable USB: un balanceo
  pequeño del cuerpo, no un recorrido amplio con los brazos.
- Si se desenchufa durante el gesto, la IncuTwin se apaga y al volver arranca normal; el gesto
  no deja nada a medias que recuperar (no da corazones ni manda nada).
- Los textos no invitan a pasear con ella ni a moverla lejos.

### Movimientos bruscos

Una sacudida, un golpe o un vaivén rápido cortan el abrazo o el achuchón en silencio: el motor
se para y no sale ningún texto. Después hay que volver a empezar.

### Dibujos nuevos

- Hoy el bebé tiene cuatro fotogramas (dormido, dos de bostezo y despierto) que
  `tools/gen_assets.py` saca de `Images/Baby.png`, en niño y niña, con los tonos de piel y la
  ropa de la tienda por paleta.
- **Referencia aprobada por Pablo (27-nov-2026):** la hoja que generó Gemini a partir de
  `Baby.png`, guardada como captura en
  `C:\Users\Pablo\Documents\IncuTwin\afecto-borrador\gemini_01_captura.png` (fuera del repo; el
  original está en el Gemini de Pablo, conversación «Secuencia de Pixel Art de Bebé»). Marca la
  dirección, no es el asset: Gemini redibuja el bebé entero.
  - Caricia: sonrisa más marcada, mofletes más sonrosados, un corazón junto a la cabeza.
  - Cosquillitas: ojos de risa (arcos hacia arriba) y boca abierta sonriendo.
  - Abrazo: cara tranquila, halo melocotón pegado al contorno del bebé y dos corazones.
  - Achuchón: dormido con la boca en «o», el mismo halo y «z Z».
- El dibujo de reposo no se toca. Las caras nuevas se pasan al dibujo de siempre como
  fotogramas (igual que los bostezos), para que valgan con todos los tonos, gorros y mantas. El
  corazón, el halo y las «z Z» van aparte del fotograma.
- Los primeros bocetos hechos por script (`afecto-borrador/00_bocetos_afecto.png`) no le
  gustaron a Pablo: no usarlos como referencia.
- Pablo los aprueba con capturas antes de darlos por buenos, como en la tienda.

### Aviso al subir de nivel

El aviso de subida de nivel que ya existe añade una línea con lo que se abre:

- Caricia: «Ahora puedes acariciarle: pasa el dedo sobre el bebé».
- Cosquillitas: «Ahora puedes hacerle cosquillitas: acaríciale rápido».
- Abrazo: «Ahora puedes abrazarle: apoya tu IncuTwin en el pecho».
- Achuchón: «Ahora puedes achucharle: abrázale y mécele despacio».

La franja de avisos de hoy (44 px, 4 s, una línea) no da para eso, así que la subida de nivel
pasa a ser una tarjeta más alta que se queda hasta que la tocas.

La tarjeta del nivel en Corazones enseña lo que abre el siguiente nivel y, al tocarla, la lista
de lo que ya tienes abierto con cómo se hace cada cosa.

### Uso

Caricias, cosquillitas, abrazos y achuchones se cuentan por día junto al resto del uso del panel
(`usage_stats`) y viajan con la telemetría de uso que ya existe. Solo son contadores.

### Placa (fase 2)

- LIS2DH12 en I2C 0x18, en el mismo bus que el táctil; INT1 en IO2. Motor en IO1.
  Detalle en `Hardware/acelerometro/README.md` de `feat/acelerometro`.
- Si al arrancar el acelerómetro no responde, Abrazo y Achuchón no existen en esa unidad: no se
  anuncian al subir de nivel y no hay error en pantalla. Queda anotado en el log.
- Pablo confirma (27-nov-2026) que todas las IncuTwin llevarán la placa, así que no se diseña
  una versión de Abrazo solo con pantalla.
- Todavía no hay ninguna placa montada en una IncuTwin (Pablo, 27-nov-2026). Por eso la fase 2
  empieza por el driver, el reconocedor en C puro probado con movimientos sintéticos y un comando
  de consola para grabar muestras. Los umbrales y las grabaciones reales para las pruebas se
  hacen cuando haya una unidad con placa; hasta entonces Abrazo y Achuchón no se publican.

### Pruebas

- La lógica (qué nivel abre qué, reconocer caricia y cosquillas a partir de puntos del dedo,
  reconocer abrazo, mecer y sacudida a partir de muestras del acelerómetro) va en C puro, sin
  LVGL ni I2C, y se prueba en `test_apps` como `hand_hold` y `rewards_logic`.
- Los tests de niveles y la spec `rewards` («Niveles») se actualizan con los nombres y umbrales
  nuevos.
- Los umbrales de movimiento se ajustan con una unidad real con placa; los de las pruebas son
  grabaciones de esa unidad (mesa, mano, pecho, mecer, sacudir).

## Decisiones mías (Pablo no las ha dicho; se pueden cambiar)

- Umbrales 0 / 10 / 30 / 75 / 150.
- Las interacciones nuevas no dan corazones ni cuentan para la racha.
- No hay regalo de corazones al subir de nivel.
- La caricia y las cosquillas no llevan botón ni sonido.
- Sin interruptor: sale con el firmware que lo lleve. Aviso único para quien ya tenga nivel.
- El latido del motor sigue al del bebé pero no es el `bpm` exacto.
- La subida de nivel es una tarjeta que se queda hasta tocarla; en Corazones se puede releer
  cómo se hace cada interacción abierta.
- Las interacciones se cuentan en la telemetría de uso.
- La «risa» de las cosquillitas es suave (Pablo mantuvo el nombre; yo propuse rebajar la
  reacción porque a un prematuro no se le hacen cosquillas).
- Abrazo de 10 s y achuchón de 20 s, sin botón previo.
- Textos de los avisos y traducciones al inglés.
- Dos fases: primero la pantalla, luego la placa.

## Lo que queda para quien lo construya

- Distancias, velocidades y tiempos exactos que separan dedo quieto, caricia y cosquillas.
- Cómo conviven la caricia y el deslizar entre páginas de la demo (hoy el deslizamiento
  horizontal cambia de página en demo).
- Frecuencia de muestreo, filtros y umbrales del acelerómetro, y la forma del pulso del motor.
- Nombre y reparto de componentes, y si la spec va como cambio de openspec (`hmi-afecto`) con
  sus deltas de `rewards` y `twin-display`.

## Preguntas abiertas para Pablo

Ninguna. Resuelto el 27-nov-2026: las caras (referencia de Gemini), la placa (aún sin montar) y
la alimentación (siempre enchufada).
