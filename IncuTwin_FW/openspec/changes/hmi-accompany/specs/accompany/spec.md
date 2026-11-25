## ADDED Requirements

### Requirement: Elegir IncuNest [test-unity]
En modo Acompañar, sin IncuNest actual, el panel SHALL elegir entre las IncuNest con bebé
(`state = "baby"` y `baby` ausente, `in` o `parents`) la de más segundos en su historial; con
empate, la de mayor `hash(client_id) ^ hash(id)`. Sin ninguna, SHALL quedarse buscando.

#### Scenario: Prefiere la que más ha acompañado
- **WHEN** hay bebé en 353 (historial 100 s) y en 297 (historial 5000 s)
- **THEN** elige 297 y pasa a "Un bebé te espera"

#### Scenario: Nadie con bebé
- **WHEN** todas las IncuNest conocidas están libres o desconectadas
- **THEN** la barra dice "Ahora no hay bebés. Te avisaré" y, al llegar un bebé a cualquiera, la elige

### Requirement: Días sin bebés [test-unity]
Un día en que el panel estuvo encendido sin ningún bebé al que acompañar (buscando, en fin de
estancia o en la despedida) y en ningún momento tuvo uno (esperando o con él) SHALL ser un día de
descanso: no suma a la racha pero tampoco la rompe, y se pinta en azul claro. Un día con el panel
apagado todo el día NO SHALL serlo.

#### Scenario: Sin bebés en medio de la racha
- **WHEN** hay corazón el lunes, el martes no hay ningún bebé en todo el día y el miércoles hay
  corazón
- **THEN** el martes sale azul claro y la frase dice "¡2 días seguidos! Vuelve mañana"

#### Scenario: Hoy no hay bebés
- **WHEN** hoy el panel solo ha estado buscando
- **THEN** la frase dice "Hoy no hay bebés: tu racha sigue a salvo"

### Requirement: Súper corazón [test-unity]
En modo Acompañar, "Coge mi mano" SHALL completarse con 20 s de dedo (`CONFIG_INCUTWIN_HAND_MIN_S`,
antes 60 s). Si en el mismo gesto se llega a 3 min de dedo, ese día SHALL ser un súper corazón:
dorado en la racha, y también cuenta como día acompañado. Mientras se sigue con el dedo, la barra
de estado SHALL decir cuánto falta ("Súper corazón en 2:40") y, al llegar, "¡Súper corazón!".

Al soltar tras completar, el gesto SHALL terminar: volver a poner el dedo en el agradecimiento NO
SHALL reanudarlo, y otro gesto empieza de cero con "Coge mi mano". El agradecimiento SHALL durar
15 s (`CONFIG_INCUTWIN_HAND_THANKS_S`) o hasta pulsar su botón "Volver al bebé".

#### Scenario: Volver a coger la mano
- **WHEN** se suelta tras completar y se vuelve a poner el dedo
- **THEN** sigue el agradecimiento; con "Volver al bebé" (o a los 15 s) vuelve la vista del bebé
  y hay que pulsar "Coge mi mano" y empezar de 0

#### Scenario: Tres minutos
- **WHEN** se completa el gesto y se mantiene el dedo hasta los 3 min
- **THEN** el corazón de hoy se pinta dorado y la frase de la racha dice "¡Súper corazón hoy! 3
  minutos de la mano"

### Requirement: Nunca una IncuNest de España [test-unity]
Las IncuNest de España son de muestra o están en fabricación. El panel SHALL ignorar los estados
cuyo `country` sea `ES`, `XX` o falte (podría ser una de aquí): no las elige, no las muestra y no
las guarda. Si la actual resulta serlo, SHALL dejarla en el acto, sin despedida ni margen de 2 h,
y buscar otra.

#### Scenario: Bebé en España y en Togo
- **WHEN** hay bebé en 363 (`country: "ES"`) y en 297 (`country: "TG"`)
- **THEN** elige 297 aunque 363 tenga más historial

#### Scenario: La actual es de España
- **WHEN** el panel acompaña a TEST-acc y llega su estado con `country: "ES"`
- **THEN** la deja y elige otra IncuNest con bebé, o se queda buscando

### Requirement: No abandonar a mitad de estancia [test-unity]
Con una IncuNest actual con bebé, el panel SHALL quedarse con ella aunque otra con más historial
tenga bebé.

#### Scenario: La favorita recibe un bebé
- **WHEN** acompaña a 353 y entra un bebé en 297, con más historial
- **THEN** sigue con 353

### Requirement: Fin de la estancia [test-unity]
Si la IncuNest actual pasa a `baby = "home"`, el panel SHALL pasar a la despedida, congelar esa
vista y elegir otra al tocar la pantalla o al cambiar el día. Si deja de tener bebé por otra causa
(libre, desconectada), SHALL esperar 2 h; si el bebé vuelve sigue con ella, y si no, elige otra.
Si tras conectar y recibir los retenidos no llega ningún estado de la actual, SHALL tratarlo igual
que un fin de estancia sin alta.

#### Scenario: Se va a casa
- **WHEN** 353 publica `baby: "home"` y después `state: "free"`
- **THEN** la pantalla sigue en "¡Se ha ido a casa! Gracias por acompañarle" hasta que se toca, y
  entonces elige otra IncuNest

#### Scenario: Corte de conexión corto
- **WHEN** 353 pasa a `offline` 30 min y vuelve con el bebé
- **THEN** el panel sigue con 353, sin buscar otra

#### Scenario: Estancia terminada sin alta
- **WHEN** 353 queda libre 2 h
- **THEN** el panel elige otra IncuNest sin haber celebrado nada

#### Scenario: La actual ya no publica
- **WHEN** el panel se reinicia acompañando a una IncuNest sin estado retenido
- **THEN** pasada la espera de los retenidos cuenta como fin de estancia y a las 2 h elige otra

### Requirement: Tiempo acompañado [test-unity]
El panel SHALL sumar 1 s por segundo a la IncuNest actual solo si hoy se ha completado
"Coge mi mano" con este bebé y el bebé está a la vista. El historial SHALL guardar hasta 32
IncuNest y, lleno, descartar la de menos tiempo que no sea la actual.

#### Scenario: Sin gesto no cuenta
- **WHEN** el bebé lleva 1 h a la vista y hoy no se ha completado el gesto
- **THEN** el historial no cambia

#### Scenario: El gesto vale para el día
- **WHEN** se completa el gesto a las 10:00 y el bebé sigue a la vista hasta las 11:00
- **THEN** se suman 3600 s; al día siguiente no se suma hasta un nuevo gesto

### Requirement: Recordatorio diario [test-unity]
A partir de las 19:00 hora de España, si hoy no se ha completado el gesto y hay bebé, el panel
SHALL llamar una vez (melodía corta, halo que llama) y mostrar "¿Le das la mano hoy?" hasta el
gesto. Entre las 22:00 y las 9:00 las melodías automáticas SHALL no sonar.

#### Scenario: Una sola llamada
- **WHEN** a las 19:00 no ha habido gesto
- **THEN** suena una vez; a las 20:00 no vuelve a sonar

### Requirement: Días acompañados [test-unity]
El panel SHALL marcar cada día con un gesto completado y mostrar en la página del país 7
corazones, llenos los días de los últimos 7 en que acompañó (el de la derecha es hoy).

#### Scenario: Página de la racha
- **WHEN** en modo Acompañar se desliza a la derecha del mapa
- **THEN** sale "Tu racha": 7 corazones de lunes (izquierda) a domingo, con su inicial debajo (hoy
  en coral), que se rellenan de izquierda a derecha según pasa la semana; los días que no han
  llegado casi transparentes; y una frase: "¡3 días seguidos! Vuelve mañana", "Cada día que le das
  la mano se enciende un corazón" o, sin hora SNTP, "Esperando la hora para contar los días"

#### Scenario: Gesto sin hora
- **WHEN** se completa "Coge mi mano" antes de que el panel tenga hora SNTP
- **THEN** en cuanto llega la hora, el gesto se apunta en el día en que se hizo y se enciende su
  corazón

#### Scenario: Racha con un hueco
- **WHEN** hubo gesto hoy, ayer y hace 3 días
- **THEN** los corazones son ○○○●○●● de izquierda a derecha
