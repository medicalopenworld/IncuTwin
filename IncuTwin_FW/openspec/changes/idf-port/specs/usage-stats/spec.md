# usage-stats

Contadores de uso del panel, acumulados de por vida. En esta versión no se publican: quedan en el
dispositivo para una futura telemetría o para diagnóstico por consola.

## ADDED Requirements

### Requirement: Contadores [test-unity]
El panel SHALL mantener cuatro contadores: `on_s` (segundos encendido), `conn_s` (segundos con
`link_ok` y `online`, sin contar la demo), `hand_s` (segundos tocando al bebé o el botón) y
`hand_n` (número de toques). Se incrementan con un tick de 1 s.

#### Scenario: Diez segundos de toque
- **WHEN** se mantiene el botón "Agarra mi mano" 10 s
- **THEN** `hand_n` sube en 1 y `hand_s` en 10 (±1)

#### Scenario: Demo no cuenta como conexión
- **WHEN** el panel pasa 60 s en modo demo sin WiFi
- **THEN** `on_s` sube 60 y `conn_s` no cambia

### Requirement: Persistencia con poco desgaste [test-unity]
Los contadores SHALL guardarse en NVS `usage` cada 600 s de encendido y leerse al arrancar.
Perder hasta 10 min de cuenta por un corte de alimentación es aceptable.

#### Scenario: Reinicio
- **WHEN** el panel lleva 25 min encendido y se reinicia
- **THEN** al arrancar `on_s` es ≥ el valor guardado a los 20 min

### Requirement: Consulta por consola [manual]
El comando serie `usage` SHALL imprimir los cuatro contadores.

#### Scenario: Lectura
- **WHEN** se escribe `usage` en el monitor serie
- **THEN** se imprimen `on_s`, `conn_s`, `hand_s`, `hand_n`
