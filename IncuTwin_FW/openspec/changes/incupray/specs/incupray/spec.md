## ADDED Requirements

### Requirement: Horario [test-unity]
La IncuPray SHALL ocurrir en la ventana que publica la API en `incupray/next` (retenido:
`{"id","start","duration_s"}`, `start` en epoch). La de serie es cada domingo a las 20:00 hora de
España con 600 s. Sin mensaje en `incupray/next` o sin hora en el panel NO SHALL haber IncuPray.
Una ventana ya pasada NO SHALL activarse.

#### Scenario: Dentro de la ventana
- **WHEN** `start` es el domingo a las 20:00, `duration_s` 600 y son las 20:04
- **THEN** la IncuPray está abierta y quedan 6:00

#### Scenario: Sin horario
- **WHEN** el panel no ha recibido `incupray/next`
- **THEN** no hay avisos ni IncuPray

### Requirement: Avisos de la IncuPray [manual]
La franja de arriba SHALL avisar la víspera a las 20:00 ("Mañana a las 20:00, IncuPray"), una hora
antes ("Dentro de una hora, IncuPray") y al empezar ("Empieza la IncuPray: toca para unirte").
Tocar la de inicio SHALL abrir la IncuPray. Dentro del horario nocturno los avisos NO SHALL sonar.
Si la IncuPray terminó sin que el panel se uniera, la primera vez que se use al día siguiente SHALL
salir el resumen ("El domingo 214 IncuTwin estuvisteis juntas"), sin reproches.

#### Scenario: Una hora antes
- **WHEN** son las 19:00 del domingo con la IncuPray a las 20:00
- **THEN** sale la franja "Dentro de una hora, IncuPray"

### Requirement: Entrar con una apuesta [test-unity]
Unirse SHALL ser posible en cualquier momento de la ventana. Al unirse por primera vez en esa
IncuPray SHALL restarse 3 corazones si el saldo es ≥ 3; con menos, la entrada SHALL ser gratis y
el saldo no cambia. Volver a entrar en la misma IncuPray NO SHALL cobrar otra vez.

#### Scenario: Con saldo
- **WHEN** el saldo es 20 y se entra en la IncuPray
- **THEN** el saldo pasa a 17 y dice "Te unes con 3 corazones"

#### Scenario: Sin saldo
- **WHEN** el saldo es 1 y se entra
- **THEN** se entra gratis y el saldo sigue en 1

### Requirement: Mapa de bebés [manual]
La pantalla de la IncuPray SHALL enseñar el mapa con todos los bebés que hay en ese momento en las
IncuNest (lista de `incupray/live`), cada uno con su cabeza de niño o niña en su país (repartidos
alrededor del punto del país si hay varios); arriba "214 IncuTwin juntas" y lo que queda ("7:12");
abajo los nombres de las IncuTwin unidas pasando de uno en uno, con el propio resaltado. «Salir»
SHALL volver a Inicio sin perder lo ganado. Los textos NO SHALL hablar de rezar.

#### Scenario: Elegir un bebé
- **WHEN** en el mapa se toca la cabeza de una niña en Togo
- **THEN** se ve a esa bebé en grande con la bandera de Togo y "Mantén el dedo para estar con ella"

### Requirement: Un corazón por minuto [test-unity]
Con un bebé abierto, mantener el dedo pulsado sobre él SHALL sumar tiempo; cada 60 s acumulados en
la misma IncuPray SHALL dar 1 corazón, hasta 10 por IncuPray. Soltar NO SHALL perder los segundos
acumulados. Al cerrarse la ventana el contador SHALL pararse aunque se siga con el dedo. Estos
corazones SHALL sumarse al saldo fuera del tope diario y al total para el nivel.

#### Scenario: Dos minutos y medio
- **WHEN** se mantiene el dedo 90 s, se suelta y luego otros 60 s
- **THEN** se han ganado 2 corazones y quedan 30 s acumulados

#### Scenario: Tope
- **WHEN** se mantiene el dedo los 10 minutos enteros y hasta el final
- **THEN** se ganan 10 corazones, no más

### Requirement: Participación y resumen [test-unity]
Unido a la IncuPray, el panel SHALL publicar en `incutwin/<id>/incupray` (no retenido, QoS 1)
`{"id","joined":true,"held_s","hearts"}` al unirse, cada 60 s y al salir o cerrarse la ventana.
Al terminar SHALL enseñar «Gracias. N IncuTwin habéis enviado M corazones» con los totales de
`incupray/summary` y los corazones ganados por el panel. Sin conexión al cerrarse, SHALL publicar
la última participación al volver la conexión, si es la misma IncuPray.

#### Scenario: Cierre
- **WHEN** termina la ventana con el panel unido tras 4 min de dedo
- **THEN** publica `{"id":"2026-12-06","joined":true,"held_s":240,"hearts":4}` y enseña el gracias

### Requirement: IncuPray de la demo [manual]
Con la demo activa SHALL poder lanzarse una IncuPray simulada de 3 minutos, con 6 bebés y 12
nombres inventados, que no publica nada ni toca el saldo de verdad.

#### Scenario: Feria
- **WHEN** en la demo se lanza la IncuPray
- **THEN** sale el mapa con 6 bebés y los nombres inventados pasando, y al terminar el saldo de
  verdad no ha cambiado
