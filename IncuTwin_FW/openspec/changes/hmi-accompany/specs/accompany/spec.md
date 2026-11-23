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

#### Scenario: Racha con un hueco
- **WHEN** hubo gesto hoy, ayer y hace 3 días
- **THEN** los corazones son ○○○●○●● de izquierda a derecha
