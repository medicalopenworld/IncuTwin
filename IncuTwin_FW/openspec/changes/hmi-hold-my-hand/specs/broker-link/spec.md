## MODIFIED Requirements

### Requirement: Disciplina de tráfico [test-unity]
El panel NO SHALL publicar nada que no esté en esta spec, en `device-status`, en `hold-my-hand`
o en los acuses propios del protocolo: ni telemetría periódica ni respuestas a comandos. El único
mensaje de "Coge mi mano" es el aviso de `hold-my-hand`, uno por gesto completado.

#### Scenario: Una hora conectado sin eventos
- **WHEN** el panel pasa 1 h conectado sin comandos ni pulsaciones
- **THEN** solo se observa un PUBLISH de `status` (el de la hora) además de los PINGREQ

#### Scenario: Toques sueltos
- **WHEN** se toca al bebé diez veces durante 5 s cada vez
- **THEN** no se publica ningún mensaje `hand`
