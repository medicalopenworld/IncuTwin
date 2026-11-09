## MODIFIED Requirements

### Requirement: Latido audible [manual]
El latido "lub-dub" SHALL sonar al ritmo `bpm` del gemelo: 330 Hz 50 ms · silencio 110 ms ·
262 Hz 40 ms · silencio hasta completar 60000/`bpm` ms (mínimo 50 ms de silencio). Suena solo
al tocar el icono del corazón de la vista BEBÉ: durante 10 s, o hasta volver a tocarlo, salir de
la vista o cambiar de página. No suena tras "Bebé detectado" ni durante el gesto "Coge mi mano".
Con `bpm = 0` o volumen Apagado no suena. El latido en pantalla es independiente (siempre late).

#### Scenario: Tocar el corazón
- **WHEN** se toca el icono del corazón con `bpm = 120`
- **THEN** el icono se ilumina y se oyen ~20 latidos (uno cada 500 ms) durante 10 s; después
  silencio

#### Scenario: Parar antes
- **WHEN** suena el latido y se vuelve a tocar el icono
- **THEN** el latido cesa

#### Scenario: Sin latido en la detección ni en el gesto
- **WHEN** suena "Bebé detectado" o se mantiene el dedo sobre el bebé en "Coge mi mano"
- **THEN** no se oye el latido

#### Scenario: Sin pulso
- **WHEN** se toca el icono del corazón con `bpm = 0`
- **THEN** no suena nada

## ADDED Requirements

### Requirement: Buzzer silenciado en compilación [manual]
Con la opción de compilación `INCUTWIN_BUZZER_MUTE` activada, el zumbador NO SHALL sonar nunca
(ni melodía de arranque, ni avisos, ni latido, ni clic), sea cual sea el volumen de ajustes. Por
defecto la opción está desactivada y el comportamiento es el de esta spec.

#### Scenario: Pruebas en silencio
- **WHEN** se arranca un firmware compilado con `INCUTWIN_BUZZER_MUTE=y` y volumen Alto
- **THEN** no suena la melodía de arranque ni ningún otro sonido
