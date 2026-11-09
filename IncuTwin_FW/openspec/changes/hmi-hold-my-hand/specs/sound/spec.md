## MODIFIED Requirements

### Requirement: Latido audible [manual]
El latido "lub-dub" SHALL sonar al ritmo `bpm` del gemelo: 330 Hz 50 ms · silencio 110 ms ·
262 Hz 40 ms · silencio hasta completar 60000/`bpm` ms (mínimo 50 ms de silencio). Suena solo
(a) durante la ventana de 3000 ms tras "Bebé detectado" y (b) mientras el dedo está sobre el bebé
en el gesto "Coge mi mano" (estados sosteniendo y completado; no en listo ni en gracia). Con
`bpm = 0` o volumen Apagado no suena. El latido en pantalla es independiente (siempre late).

#### Scenario: Ventana tras detección
- **WHEN** termina la melodía "Bebé detectado" con `bpm = 120`
- **THEN** se oyen ~6 latidos (uno cada 500 ms) y después silencio

#### Scenario: Toque con pulso
- **WHEN** se toca al bebé 2 s con `bpm = 150`
- **THEN** se oyen ~5 latidos (uno cada 400 ms) y cesan al soltar

#### Scenario: Toque sin pulso
- **WHEN** se toca al bebé con `bpm = 0`
- **THEN** no suena nada

## ADDED Requirements

### Requirement: Buzzer silenciado en compilación [manual]
Con la opción de compilación `INCUTWIN_BUZZER_MUTE` activada, el zumbador NO SHALL sonar nunca
(ni melodía de arranque, ni avisos, ni latido, ni clic), sea cual sea el volumen de ajustes. Por
defecto la opción está desactivada y el comportamiento es el de esta spec.

#### Scenario: Pruebas en silencio
- **WHEN** se arranca un firmware compilado con `INCUTWIN_BUZZER_MUTE=y` y volumen Alto
- **THEN** no suena la melodía de arranque ni ningún otro sonido
