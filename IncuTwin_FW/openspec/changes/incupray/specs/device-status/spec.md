## ADDED Requirements

### Requirement: Nombre en el estado [test-unity]
Si la IncuTwin tiene nombre puesto en Ajustes, el payload de `incutwin/<id>/status` SHALL llevar
`"name"` con ese texto (UTF-8, ≤ 16 caracteres); sin nombre NO SHALL llevar el campo. Cambiar el
nombre SHALL publicar `status` en ≤ 5 s.

#### Scenario: Con nombre
- **WHEN** el nombre es "Familia García"
- **THEN** `status` incluye `"name":"Familia García"` junto a `online`, `fw`, `rssi`
