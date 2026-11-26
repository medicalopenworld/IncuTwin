## ADDED Requirements

### Requirement: Métricas de uso y recompensas [test-unity]
El panel SHALL publicar en `incutwin/<id>/metrics` (no retenido, QoS 1) un JSON con los contadores
de uso (`on_s`, `conn_s`, `hand_s`, `hand_n`) y las recompensas (`hearts`, `earned`, `level` 0..4,
`streak`, `saved`, `days`, `babies`, `seals` en bits y `seals_n`, `countries` como "TG,CM",
`countries_n`, `stars`, `unlocked` en bits, `halo`, `bg`, `garland`, `named_incunest`) y `ts`
(epoch; 0 sin hora). SHALL publicarlo al conectar con el broker, una vez por hora y 30 s después del
último cambio de recompensas (varios cambios seguidos salen en un solo mensaje). Lo de la demo NO
SHALL publicarse. El IoT Gateway lo lleva a ThingsBoard como telemetría del dispositivo IncuTwin.

#### Scenario: Gesto con sello
- **WHEN** un gesto suma un corazón y concede «Primera mano»
- **THEN** a los 30 s sale un solo `metrics` con `"hearts":1` y `"seals_n":1`

#### Scenario: Pasaporte lleno
- **WHEN** el pasaporte tiene 32 países y los contadores tienen su valor máximo
- **THEN** el payload sigue cabiendo y es JSON válido

#### Scenario: Demo
- **WHEN** la demo está activa y cambia su saldo simulado
- **THEN** no sale ningún `metrics` con ese saldo
