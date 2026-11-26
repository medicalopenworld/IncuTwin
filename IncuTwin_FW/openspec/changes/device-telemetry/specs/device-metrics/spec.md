## ADDED Requirements

### Requirement: Campos de la foto para analítica [test-unity]
El payload de `incutwin/<id>/metrics` SHALL llevar además `gen`, `fw`, `inc` (IncuNest acompañada o
`""`), `phase` (`off|searching|waiting|with|ending|farewell`), `lang`, `vol` (0..3), `bright`
(10..100), `demo_n`, `boot_n`, `q` (eventos en cola) y `lost` (eventos descartados), y SHALL seguir
cabiendo con el pasaporte lleno y todos los contadores al máximo. Lo lee el recolector del VPS
(rol `collector`), no el IoT Gateway.

#### Scenario: Acompañando
- **WHEN** el panel acompaña a la 353 con el bebé dentro, `gen` 1 y 3 eventos en cola
- **THEN** el payload contiene `"gen":1`, `"inc":"353"`, `"phase":"with"` y `"q":3`

#### Scenario: Buscando
- **WHEN** el panel no tiene IncuNest
- **THEN** el payload contiene `"inc":""` y `"phase":"searching"`
