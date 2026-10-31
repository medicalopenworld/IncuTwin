# sound

Notificaciones retro (onda cuadrada, estilo 8 bits) por el zumbador. Todo no bloqueante; una
melodía en curso tiene prioridad sobre el latido.

Notas (Hz): C4 262 · E4 330 · C5 523 · E5 659 · G5 784 · C6 1047 · E6 1319 · G6 1568 · C7 2093.

## ADDED Requirements

### Requirement: Melodías [manual]
El panel SHALL disponer de estas melodías (frecuencia Hz, duración ms; 0 Hz = silencio):

| Nombre | Secuencia | Cuándo |
|---|---|---|
| Arranque | C5 70 · E5 70 · G5 70 · C6 180 | al arrancar, con el splash |
| Bebé detectado | G5 90 · C6 90 · E6 220 | transición del gemelo a `baby = in` (ver `twin-state-ingest`) |
| Con sus papás | C5 80 E5 80 G5 80 C6 80 · E5 80 G5 80 C6 80 E6 80 · G5 80 C6 80 E6 80 G6 200 · 0 60 · G6 80 · 0 40 · C7 400 | transición a `baby ∈ {parents, home}` |
| Prueba | C6 120 | cambio de volumen, entrada/salida de demo, `cmd/test_melody` |

#### Scenario: Bebé detectado
- **WHEN** el gemelo pasa de `baby = none` a `baby = in` con volumen Alto
- **THEN** suenan tres notas ascendentes (~400 ms en total) y a continuación el latido audible
  durante 3000 ms (si `bpm > 0`)

#### Scenario: Fanfarria
- **WHEN** el gemelo pasa a `baby = parents`
- **THEN** suena la fanfarria completa (~1,9 s) una sola vez

### Requirement: Latido audible [manual]
El latido "lub-dub" SHALL sonar al ritmo `bpm` del gemelo: 330 Hz 50 ms · silencio 110 ms ·
262 Hz 40 ms · silencio hasta completar 60000/`bpm` ms (mínimo 50 ms de silencio). Suena solo
(a) durante la ventana de 3000 ms tras "Bebé detectado" y (b) mientras el usuario toca al bebé o
el botón "Agarra mi mano". Con `bpm = 0` o volumen Apagado no suena. El latido en pantalla es
independiente (siempre late).

#### Scenario: Ventana tras detección
- **WHEN** termina la melodía "Bebé detectado" con `bpm = 120`
- **THEN** se oyen ~6 latidos (uno cada 500 ms) y después silencio

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
