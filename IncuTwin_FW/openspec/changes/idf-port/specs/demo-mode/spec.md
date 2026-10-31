# demo-mode

Enseñar el panel sin WiFi ni incubadora, con el botón BOOT. Va en el firmware normal.

## ADDED Requirements

### Requirement: Botón BOOT [test-unity]
Con la pantalla principal activa (no durante el onboarding), el panel SHALL muestrear el botón
BOOT con antirrebote de 30 ms y distinguir: pulsación larga (≥ 2000 ms, se dispara sin soltar) y
pulsación corta (soltar antes de 2000 ms).

#### Scenario: Larga sin soltar
- **WHEN** se mantiene BOOT 2 s
- **THEN** la acción de pulsación larga ocurre a los 2 s aunque el botón siga pulsado, y al soltar
  no se cuenta además una corta

### Requirement: Entrar y salir [manual]
La pulsación larga SHALL alternar el modo demo y hacer sonar el pitido de prueba. Al entrar, el
panel SHALL mostrar el primer escenario con la insignia DEMO. Al salir, SHALL volver a mostrar el
estado real actual (incluidos los mensajes recibidos durante la demo). El modo demo NO SHALL
afectar a la conexión MQTT ni a los contadores de uso.

#### Scenario: Entrar sin WiFi
- **WHEN** el panel muestra "Sin conexión WiFi" y se mantiene BOOT 2 s
- **THEN** suena un pitido, aparece "DEMO" arriba a la izquierda y se ve el bebé durmiendo

#### Scenario: Salir con estado real cambiado
- **WHEN** durante la demo llega por MQTT `state: free` y se sale de la demo
- **THEN** la pantalla muestra la incubadora vacía (el estado real), no el escenario

### Requirement: Escenarios [manual]
La pulsación corta en demo SHALL pasar al siguiente escenario, en bucle, en este orden:

| # | id | Lo que se ve |
|---|---|---|
| 1 | sleep | bebé durmiendo, 120 lpm, calor estable |
| 2 | awake | bebé despierto, 140 lpm |
| 3 | heating | calentando (halo ámbar), 130 lpm |
| 4 | alarm | ¡Alarma! (halo rojo rápido), 180 lpm |
| 5 | photo | fototerapia (halo azul), 130 lpm |
| 6 | parents | con sus papás |
| 7 | home | ¡ya está en casa! |
| 8 | empty | IncuNest sin bebé |
| 9 | off | IncuNest apagada |
| 10 | unlinked | sin IncuNest vinculada |

Cada escenario SHALL producir las mismas transiciones sonoras que si llegara por red (entrar en
`sleep` desde `unlinked` suena "Bebé detectado"; entrar en `parents` suena la fanfarria).
Fuera de demo la pulsación corta no hace nada.

#### Scenario: Ciclo completo
- **WHEN** se pulsa BOOT brevemente 10 veces desde el escenario 1
- **THEN** se recorren los 10 escenarios y se vuelve a "sleep", que vuelve a sonar

#### Scenario: Corta fuera de demo
- **WHEN** se pulsa BOOT brevemente sin demo activa
- **THEN** no pasa nada

### Requirement: Modo descarga intacto [manual]
Pulsar BOOT durante el reinicio SHALL seguir metiendo la placa en modo descarga del ROM; el
modo demo solo lee el botón con el firmware ya arrancado.

#### Scenario: Flasheo
- **WHEN** se mantiene BOOT y se pulsa RESET
- **THEN** `idf.py flash` encuentra el puerto en modo descarga
