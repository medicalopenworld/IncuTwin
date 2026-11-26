## MODIFIED Requirements

### Requirement: Métricas de uso y recompensas [test-unity]
`incutwin/<id>/metrics` SHALL llevar además `hat_boy`, `hat_girl`, `blanket` (artículo elegido),
`santa` y `snow` (0/1, puestos); `unlocked` SHALL ser
el número de 64 bits de desbloqueos.

#### Scenario: Gorro Menta de niño
- **WHEN** el gorro de niño es Menta
- **THEN** el payload lleva `"hat_boy":<id de Menta>`
