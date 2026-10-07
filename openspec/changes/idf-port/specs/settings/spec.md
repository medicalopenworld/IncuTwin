# settings

Pantalla de ajustes y persistencia de preferencias del panel. Todo lo que aquí se cambia
sobrevive al reinicio; nada requiere teclear.

## ADDED Requirements

### Requirement: Idioma [manual]
La pantalla de ajustes SHALL ofrecer dos botones "Español" / "English". Al elegir uno, todos los
textos del panel (ajustes, barra de estado, botón de la mano, portal cautivo) SHALL cambiar al
instante y la elección SHALL persistir en NVS. Por defecto: español.

#### Scenario: Cambio a inglés
- **WHEN** se pulsa "English" y después se vuelve a la pantalla principal
- **THEN** la barra de estado está en inglés (p. ej. "Baby sleeping") y tras reiniciar el panel
  sigue en inglés

### Requirement: Volumen en cuatro niveles [manual]
Ajustes SHALL ofrecer cuatro botones de volumen: Apagado / Bajo / Medio / Alto (Off / Low / Mid /
High). Al elegir un nivel distinto de Apagado SHALL sonar un pitido corto de prueba a ese
volumen. El nivel SHALL persistir en NVS. Por defecto: Alto.

#### Scenario: Bajar a Bajo
- **WHEN** se pulsa "Bajo"
- **THEN** suena un pitido de ~120 ms claramente más flojo que en "Alto", el botón "Bajo" queda
  resaltado y tras reiniciar el volumen sigue en Bajo

#### Scenario: Apagar
- **WHEN** se pulsa "Apagado" mientras suena una melodía
- **THEN** el sonido cesa en < 50 ms y no suena pitido de prueba

### Requirement: Brillo de pantalla [manual]
Ajustes SHALL ofrecer un control de brillo con rango 10–100 % (nunca 0: la pantalla no puede
quedar a oscuras sin forma táctil de recuperarla). El cambio SHALL aplicarse en tiempo real y
persistir en NVS. Por defecto: 100 %. El mismo valor es el que fija el comando `brightness` (ver
`device-commands`).

#### Scenario: Bajar brillo
- **WHEN** se arrastra el control al 30 %
- **THEN** la retroiluminación baja de forma visible mientras se arrastra y tras reiniciar sigue
  al 30 %

#### Scenario: Comando y control coherentes
- **WHEN** llega `cmd/brightness {"value": 60}` con la pantalla de ajustes abierta
- **THEN** el control de brillo pasa a mostrar 60 %

### Requirement: Tono de piel informativo [manual]
Ajustes SHALL mostrar los 6 tonos de piel como círculos de color con el tono actual resaltado y el
texto "Según el país de la IncuNest asignada" / "Based on the assigned IncuNest's country". Los
círculos NO son pulsables: el tono lo fija el estado recibido.

#### Scenario: Tono recibido
- **WHEN** el estado de la incubadora trae `skin = 2`
- **THEN** en ajustes queda resaltado el tercer círculo y tocarlo no cambia nada

### Requirement: Pie de información [manual]
Ajustes SHALL mostrar, en letra pequeña: "WiFi: <IP>" o "WiFi: Sin conexión"; "Servidor:
Conectado" / "Servidor: Sin conexión"; "IncuNest: <incubator_id>" o "IncuNest: —"; y "<serie> ·
v<versión de firmware>".

#### Scenario: Panel emparejado y conectado
- **WHEN** el panel tiene IP 192.168.1.40, sesión MQTT y `incubator_id = 353`
- **THEN** el pie muestra las cuatro líneas con esos valores y la versión del firmware en marcha

### Requirement: Restablecer de fábrica [manual]
Mantener pulsado el engranaje de la pantalla principal ≥ 1000 ms SHALL abrir un diálogo
"¿Restablecer de fábrica?" / "Factory reset?" con botones "Reset" y cerrar. Los diálogos del
panel SHALL usar las fuentes con glifos del español (sin cuadros de sustitución) y un ancho de
208 px centrado, sin llegar a los bordes de la pantalla. Confirmar SHALL
borrar la configuración de usuario (WiFi, onboarding completado, idioma, volumen, brillo, estado
del gemelo cacheado) y reiniciar al onboarding. NO SHALL borrar la identidad de fábrica (serie,
credenciales MQTT) ni los contadores de uso.

#### Scenario: Reset confirmado
- **WHEN** se mantiene el engranaje 1 s, aparece el diálogo y se pulsa "Reset"
- **THEN** el panel reinicia en < 2 s, arranca en la pantalla de idioma del onboarding, y tras
  completarlo vuelve a conectar al broker con las mismas credenciales de siempre

#### Scenario: Reset cancelado
- **WHEN** aparece el diálogo y se pulsa cerrar
- **THEN** el diálogo desaparece y nada cambia

### Requirement: Persistencia agrupada [test-unity]
Las preferencias (idioma, volumen, brillo) SHALL guardarse en el namespace NVS `settings` y
leerse una sola vez al arrancar; valores ausentes o fuera de rango SHALL sustituirse por los
valores por defecto (ES, Alto, 100 %).

#### Scenario: NVS corrupta
- **WHEN** `settings/vol` contiene 9
- **THEN** el panel arranca con volumen Alto y reescribe el valor por defecto
