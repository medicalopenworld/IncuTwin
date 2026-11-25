# sound

## Purpose

Notificaciones retro (onda cuadrada, estilo 8 bits) por el zumbador. Todo no bloqueante; una
melodía en curso tiene prioridad sobre el latido.

Notas (Hz): C4 262 · E4 330 · C5 523 · E5 659 · G5 784 · C6 1047 · E6 1319 · G6 1568 · C7 2093.

## Requirements

### Requirement: Melodías [manual]
El panel SHALL disponer de estas melodías (frecuencia Hz, duración ms; 0 Hz = silencio):

| Nombre | Secuencia | Cuándo |
|---|---|---|
| Arranque | C5 70 · E5 70 · G5 70 · C6 180 | al arrancar con el splash, solo tras un reinicio por encendido (`ESP_RST_POWERON`) |
| Bebé detectado | G5 90 · C6 90 · E6 220 | aviso de ingreso: `baby` pasa de `none`/`home` a `in` o `parents` (ver `event-history`) |
| Con sus papás | C5 80 E5 80 G5 80 C6 80 · E5 80 G5 80 C6 80 E6 80 · G5 80 C6 80 E6 80 G6 200 · 0 60 · G6 80 · 0 40 · C7 400 | aviso «con sus papás»: `baby` pasa a `parents` desde `in` |
| Prueba | C6 120 | cambio de volumen, entrada/salida de demo, `cmd/test_melody` |

#### Scenario: Bebé detectado
- **WHEN** el gemelo pasa de `baby = none` a `baby = in` con volumen Alto
- **THEN** suenan tres notas ascendentes (~400 ms en total) y nada más

#### Scenario: Fanfarria
- **WHEN** el gemelo pasa de `baby = in` a `baby = parents`
- **THEN** suena la fanfarria completa (~1,9 s) una sola vez

#### Scenario: Cambios en silencio
- **WHEN** el bebé pasa a `home`, vuelve de `parents` a `in`, sale (`none`) o cambian calor o
  fototerapia
- **THEN** no suena ninguna melodía (solo se ve el aviso en pantalla)

### Requirement: Latido audible [manual]
El latido "lub-dub" SHALL sonar al ritmo `bpm` del gemelo: 330 Hz 50 ms · silencio 110 ms ·
262 Hz 40 ms · silencio hasta completar 60000/`bpm` ms (mínimo 50 ms de silencio). Suena solo
mientras el usuario toca al bebé o el botón "Agarra mi mano" (ya no hay ventana de latido tras
"Bebé detectado"). Con `bpm = 0` o volumen Apagado no suena. El latido en pantalla es
independiente (siempre late).

#### Scenario: Sin ventana tras detección
- **WHEN** termina la melodía "Bebé detectado" con `bpm = 120`
- **THEN** no suena ningún latido hasta que alguien toque al bebé

#### Scenario: Toque con pulso
- **WHEN** se toca al bebé 2 s con `bpm = 150`
- **THEN** se oyen ~5 latidos (uno cada 400 ms) y cesan al soltar

#### Scenario: Toque sin pulso
- **WHEN** se toca al bebé con `bpm = 0`
- **THEN** no suena nada

### Requirement: Prioridad y reanudación [test-unity]
Una melodía en curso SHALL silenciar el latido; al terminar la melodía, si el latido sigue
activo, SHALL reanudarse desde un "lub" limpio. Lanzar una melodía mientras otra suena SHALL
sustituirla desde su primera nota.

#### Scenario: Melodía sobre latido
- **WHEN** el usuario toca al bebé y durante el toque llega la transición a `parents`
- **THEN** el latido se corta, suena la fanfarria y, si sigue tocando, el latido vuelve con "lub"

### Requirement: Clic táctil [manual]
Cada pulsación sobre un botón (incluidos los de los diálogos y el icono WiFi) SHALL producir un
tic de 25 ms (A6, 1760 Hz) al volumen configurado; con volumen Apagado no suena. El tic NO SHALL
interrumpir una melodía en curso ni sonar al tocar al bebé (que tiene su latido).

#### Scenario: Pulsar en ajustes
- **WHEN** se pulsa "English" con volumen Medio
- **THEN** se oye un tic breve y el idioma cambia

#### Scenario: Volumen apagado
- **WHEN** se pulsa cualquier botón con volumen Apagado
- **THEN** no suena nada

### Requirement: Volumen [manual]
Cuatro niveles (0 Apagado, 1 Bajo, 2 Medio, 3 Alto) SHALL fijar el ciclo de trabajo del PWM
(10 bits) con valores iniciales {0, 8, 20, 512}, a calibrar en hardware para que Bajo sea audible
en una habitación en silencio y Alto se oiga desde otra habitación. Pasar a Apagado SHALL cortar
cualquier sonido en curso de inmediato.

#### Scenario: Tres sonoridades distinguibles
- **WHEN** se reproduce el pitido de prueba en Bajo, Medio y Alto
- **THEN** una persona a 1 m distingue tres sonoridades crecientes

### Requirement: Sin sonidos espurios [manual]
No SHALL sonar nada al arrancar antes de la melodía de arranque (ningún "clic" del PWM al
inicializar), ni al cambiar el brillo, ni al reiniciar por OTA.

#### Scenario: Arranque limpio
- **WHEN** se enciende el panel
- **THEN** el primer sonido audible es la melodía de arranque

### Requirement: Horario nocturno [manual]
El panel SHALL tener un único horario nocturno: activado/desactivado y horas «desde» y «hasta»
(horas enteras, intervalo [desde, hasta); si desde > hasta cruza la medianoche; desde = hasta o
apagado = nunca), por defecto **activado de 21 a 9**, en hora de España
(`CET-1CEST,M3.5.0,M10.5.0/3`). Se guarda en NVS (namespace `settings`). Sin hora válida (SNTP
aún no sincronizado) NO SHALL considerarse de noche. El horario se evalúa en el momento de cada
petición de melodía automática, no con un valor en caché. (El cálculo `quiet_hours_active` está cubierto por
`test-unity`; el silenciado de las melodías es `manual`.)

De noche SHALL quedar en silencio toda melodía que la persona no haya provocado tocando la pantalla:
melodías de aviso (ingreso, con sus papás), melodías de comandos remotos y recordatorios del modo
acompañar (que usa este mismo horario, antes 22–9 fijo). Seguirán sonando el clic táctil, el latido
al tocar al bebé, el pitido de prueba de volumen y el de entrar o salir de la demo (las melodías de los escenarios de la demo sí se silencian). El silencio no afecta a lo
que se ve: el aviso en pantalla aparece igual.

#### Scenario: Ingreso de noche
- **WHEN** son las 23:00, el horario está activado y llega un aviso de ingreso
- **THEN** se ve la franja del aviso, no suena «Bebé detectado» y el log dice que se silencia

#### Scenario: Cruce de medianoche
- **WHEN** el horario es de 21 a 9 y son las 2:00
- **THEN** es de noche; a las 9:00 ya no lo es

#### Scenario: Pulsar de noche
- **WHEN** es de noche y se pulsa un botón
- **THEN** suena el clic

### Requirement: Melodía de arranque solo al enchufar [manual]
La melodía de arranque SHALL sonar únicamente cuando el motivo de reinicio es `ESP_RST_POWERON`, a
cualquier hora. NO SHALL sonar tras un reinicio por software, OTA, comando remoto, caída ni
perro guardián. El log del arranque SHALL indicar el motivo y si suena o no.

#### Scenario: Reinicio por OTA
- **WHEN** el panel reinicia tras una OTA
- **THEN** el splash se ve sin melodía y el log dice "sin melodia de arranque"
