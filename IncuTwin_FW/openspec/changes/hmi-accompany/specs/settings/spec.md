## REMOVED Requirements

### Requirement: Tono de piel informativo
**Reason**: no aporta a quien acompaña y ocupa media pantalla de ajustes.
**Migration**: el tono sigue llegando en el estado y se usa para pintar al bebé; solo deja de
mostrarse en ajustes.

## MODIFIED Requirements

### Requirement: Pie de información [manual]
Ajustes SHALL mostrar, en letra pequeña: "WiFi: <IP>" o "WiFi: Sin conexión"; "Servidor:
Conectado" / "Servidor: Sin conexión"; y "<serie> · v<versión de firmware>". El número de la
IncuNest NO SHALL mostrarse (en modo Acompañar cambia, y no le dice nada a quien acompaña; se ve
por la consola con `info`).

#### Scenario: Panel conectado
- **WHEN** el panel tiene IP 192.168.1.40 y sesión MQTT
- **THEN** el pie muestra la red y el servidor en una línea y el serie con la versión del firmware
  en otra, sin número de IncuNest
