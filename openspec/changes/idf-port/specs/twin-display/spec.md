# twin-display

Pantalla principal del gemelo: lo que ve la familia. Sin números clínicos; estados con color,
iconos grandes y una línea de texto siempre visible. Pensada para niños y personas mayores.

Vocabulario del modelo del gemelo usado en esta spec:

- `wifi`: el panel tiene IP.
- `broker_once`: el panel ha conectado al broker al menos una vez desde el arranque.
- `broker_lost`: el panel lleva ≥ 600 s sin sesión con el broker (tras haberla tenido).
- `paired`: hay `incubator_id` emparejado.
- `state_rx`: se ha recibido al menos un estado de la incubadora emparejada desde la última
  suscripción.
- `online`: el último estado recibido no es `offline`.
- `link_ok` = `wifi` ∧ `broker_once` ∧ ¬`broker_lost` ∧ `paired` ∧ `state_rx` ∧ `online`.
- `baby` ∈ {none, in, parents, home}; `thermo` ∈ {off, heating, stable, alarm}; `photo`;
  `bpm` (0 = sin pulso); `awake`; `skin` 0..5; `name`, `weight_g`, `age_d` (opcionales).
- `show_baby` = `link_ok` ∧ `baby` = in. `parents_view` = `link_ok` ∧ `baby` ∈ {parents, home}.

## ADDED Requirements

### Requirement: Splash de arranque [manual]
Al arrancar, el panel SHALL mostrar durante 2500 ms una pantalla blanca con el logo de IncuTwin
centrado y un indicador de actividad (spinner coral) abajo, y después SHALL fundir (400 ms) a la
pantalla principal. Durante el splash suena la melodía de arranque (ver `sound`).

#### Scenario: Arranque normal
- **WHEN** el panel arranca con onboarding ya completado
- **THEN** el logo se ve ≥ 2 s, la transición a la pantalla principal es un fundido y la pantalla
  principal ya refleja el estado conocido (sin parpadeo de estados intermedios)

### Requirement: Tres vistas del centro de la pantalla [manual]
La zona central SHALL mostrar exactamente una de tres vistas:

- **BEBÉ** cuando `show_baby`: bebé animado con halo, fila de tres iconos de estado y botón
  "Agarra mi mano".
- **CON SUS PAPÁS** cuando `parents_view`: imagen a pantalla completa del bebé con sus padres.
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

### Requirement: Barra de estado con prioridades [test-unity]
La barra inferior SHALL mostrar una única línea de texto, elegida por el primer caso que se
cumpla de esta lista, con su color:

| # | Condición | ES | EN | Color |
|---|---|---|---|---|
| 1 | `parents_view` ∧ `baby = home` | ¡Ya está en casa! | Home at last! | verde |
| 2 | `parents_view` | Con sus papás | With parents | verde |
| 3 | ¬`wifi` | Sin conexión WiFi | No WiFi connection | rojo |
| 4 | sin credenciales MQTT de fábrica | Panel sin serializar | Panel not provisioned | rojo |
| 5 | ¬`broker_once` | Conectando con el servidor... | Connecting to the server... | ámbar |
| 6 | `broker_lost` | Sin conexión con el servidor | No connection to the server | rojo |
| 7 | ¬`paired` | Sin IncuNest vinculada | No IncuNest linked | gris |
| 8 | ¬`state_rx` | Esperando a la IncuNest... | Waiting for the IncuNest... | gris |
| 9 | ¬`online` | IncuNest apagada | IncuNest off | gris |
| 10 | `baby ≠ in` | IncuNest sin bebé | IncuNest empty | azul |
| 11 | `thermo = alarm` | ¡Alarma! | Alarm! | rojo |
| 12 | `name` no vacío | `<name> · <weight_g> g · <age_d> día(s)` (solo los campos presentes) | idem con day/days | verde |
| 13 | `awake` | Bebé despierto | Baby awake | verde |
| 14 | — | Bebé durmiendo | Baby sleeping | verde |

El borde de la barra SHALL tomar el mismo color que el texto. Un nombre largo SHALL truncarse
con puntos suspensivos sin desbordar la barra.

#### Scenario: Caída breve del broker no alarma
- **WHEN** el panel mostraba "Bebé durmiendo" y pierde la sesión MQTT durante 300 s con WiFi
  activo
- **THEN** la barra sigue diciendo "Bebé durmiendo" y el bebé sigue visible (solo cambia el icono
  WiFi a ámbar)

#### Scenario: Caída larga del broker
- **WHEN** la sesión MQTT lleva 600 s perdida
- **THEN** la barra pasa a "Sin conexión con el servidor" en rojo y la vista central es VACÍA

#### Scenario: Recién emparejada sin estado
- **WHEN** llega `cmd/pair` con una incubadora y aún no se ha recibido su estado retenido
- **THEN** la barra dice "Esperando a la IncuNest..." en gris

#### Scenario: Nombre compartido
- **WHEN** `show_baby` con `name = "Lucía"`, `weight_g = 1250`, `age_d = 3`
- **THEN** la barra dice "Lucía · 1250 g · 3 días" en verde; con `age_d = 1` dice "1 día"

#### Scenario: Alarma antes que nombre
- **WHEN** `show_baby`, `thermo = alarm` y hay nombre
- **THEN** la barra dice "¡Alarma!" en rojo

### Requirement: Iconos de estado por color [manual]
En la vista BEBÉ, tres iconos circulares (termómetro, lámpara, corazón) SHALL indicar el estado
solo por el color del borde:

- Termómetro: ámbar si `thermo = heating`, verde si `stable`, rojo si `alarm`, gris si `off`.
- Lámpara: azul si `photo`, gris si no.
- Corazón: coral si `bpm > 0`, gris si no.

#### Scenario: Fototerapia con pulso
- **WHEN** `show_baby`, `photo = true`, `bpm = 130`, `thermo = stable`
- **THEN** termómetro verde, lámpara azul, corazón coral

### Requirement: Halo que respira [manual]
Detrás del bebé SHALL haber un halo circular cuya opacidad oscila 20 %→60 %→20 % con curva
suave, con color y periodo según el estado:

| Estado | Color | Periodo ida |
|---|---|---|
| `alarm` | rojo suave `0xF3B0AA` | 700 ms |
| `photo` | azul `0x8FBBF0`, fondo de pantalla `0xE8F1FB` | 2000 ms |
| `heating` | ámbar `0xF7D9A6` | 2600 ms |
| resto | azul calma `0xBBD8F2` | 2600 ms |

Prioridad: alarma > fototerapia > calentando > calma. Sin `show_baby` el halo SHALL ocultarse y el
fondo volver a `0xF4F3F0`.

#### Scenario: Entrar en alarma
- **WHEN** `thermo` pasa de `stable` a `alarm`
- **THEN** el halo cambia a rojo y su pulsación se acelera visiblemente (≈ 1,4 s por ciclo)

### Requirement: Bebé animado [manual]
El bebé SHALL animarse de forma continua:

- Respiración: desplazamiento vertical de 3 px, ida y vuelta de 2600 ms cada una.
- Bostezo: secuencia de fotogramas yawn1 (320 ms), yawn2 (900 ms), yawn2 (350 ms), yawn1
  (320 ms); el primero entre 15 y 25 s tras aparecer y después cada 18–40 s (aleatorio).
- Dormido (`awake = false`): ojos cerrados y tres "z Z z" que flotan 14 px hacia arriba mientras
  se desvanecen, en 2400 ms, escalonadas 800 ms.
- Despierto: fotograma con ojos abiertos, sin Zzz.
- Corazón: imagen sobre el pecho que late a `bpm` reales (periodo 60000/`bpm` ms, mínimo
  250 ms): opacidad 40 %→100 % en un tercio del periodo y vuelta en los dos tercios restantes.
  Con `bpm = 0` el corazón SHALL ocultarse. Nunca se muestra el número.
- Tono de piel: uno de 6 (`skin` 0..5) aplicado por intercambio de paleta, cambio instantáneo.

#### Scenario: Cambio de ritmo
- **WHEN** `bpm` pasa de 120 a 180
- **THEN** el latido visible se acelera en el siguiente ciclo, sin parpadeos ni saltos de posición

#### Scenario: Cambio de tono
- **WHEN** llega `skin = 4`
- **THEN** el bebé cambia de tono en < 100 ms y los demás colores (manta, contorno) no varían

### Requirement: Agarra mi mano [manual]
Tocar al bebé o pulsar el botón "Agarra mi mano" / "Hold my hand" SHALL, mientras dure el toque:
mostrar al bebé despierto, hacer sonar el latido (ver `sound`) y contar como interacción (ver
`usage-stats`). Al soltar, el bebé SHALL volver a su estado `awake` real. No se envía nada al
broker en esta versión.

#### Scenario: Toque de 4 s
- **WHEN** se mantiene el dedo sobre el bebé 4 s con `bpm = 140`
- **THEN** el bebé abre los ojos al instante, suena el latido a 140 lpm durante los 4 s, y al
  soltar vuelve a dormido (si `awake = false`) y el latido audible cesa en < 200 ms

### Requirement: Icono de cobertura WiFi [manual]
El icono superior izquierdo SHALL mostrar: tachado gris sin WiFi; 0–3 barras según RSSI
(≥ −55 dBm: 3; ≥ −67: 2; ≥ −78: 1; menos: 0) con WiFi; y teñido de ámbar cuando hay WiFi pero
no hay sesión con el broker. Se actualiza al menos cada 1 s.

#### Scenario: Broker caído
- **WHEN** hay WiFi con RSSI −60 dBm y la sesión MQTT se pierde
- **THEN** el icono muestra 2 barras teñidas de ámbar; al recuperar la sesión vuelve al color normal

### Requirement: Insignia DEMO [manual]
Mientras el modo demo esté activo (ver `demo-mode`), el icono WiFi SHALL sustituirse por una
insignia "DEMO" coral, para que nadie confunda los datos mostrados con datos reales.

#### Scenario: Entrar y salir de demo
- **WHEN** se activa el modo demo
- **THEN** la insignia DEMO aparece donde estaba el icono WiFi; al salir, reaparece el icono WiFi
  con la cobertura real

### Requirement: Acceso a ajustes [manual]
Una pulsación corta del engranaje SHALL abrir la pantalla de ajustes con animación de
desplazamiento (220 ms); "Volver" SHALL regresar con la animación inversa.

#### Scenario: Ida y vuelta
- **WHEN** se pulsa el engranaje y luego "Volver"
- **THEN** la pantalla principal reaparece con el estado al día (no congelado en el momento de
  salir)

### Requirement: Refresco del estado en pantalla [test-unity]
La pantalla SHALL reflejar cualquier cambio del modelo del gemelo en ≤ 300 ms y SHALL
re-evaluar las condiciones dependientes del tiempo (`broker_lost`) al menos cada 1 s.

#### Scenario: Latencia de actualización
- **WHEN** el modelo cambia `baby` de none a in
- **THEN** la vista BEBÉ está dibujada antes de 300 ms
