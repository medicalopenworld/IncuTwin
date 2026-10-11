## ADDED Requirements

### Requirement: Publicación de los contadores [test-unity]
Los cuatro contadores SHALL salir en cada `status` (ver `device-status`) y en cada informe `diag`
(ver `device-diagnostics`) con los valores vigentes en memoria, no con los últimos guardados en
NVS.

#### Scenario: Status a los cinco minutos
- **WHEN** el panel arranca con `on_s = 1000` guardado y publica `status` a los 300 s
- **THEN** el `status` lleva `"on_s":1300` (±2) aunque la NVS siga en 1000
