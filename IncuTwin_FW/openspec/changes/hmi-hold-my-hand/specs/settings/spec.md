## MODIFIED Requirements

### Requirement: Tono de piel informativo [manual]
Ajustes SHALL mostrar los 5 tonos de piel como círculos de color con el tono actual resaltado y el
texto "Según el país de la IncuNest asignada" / "Based on the assigned IncuNest's country". Los
círculos NO son pulsables fuera de demo: el tono lo fija el estado recibido. En demo SHALL ser
pulsables (ver `demo-mode`, "Probar el tono de piel en demo") y el texto SHALL ser "Demo: toca un
tono para probarlo" / "Demo: tap a tone to try it" en coral.

#### Scenario: Tono recibido
- **WHEN** el estado de la incubadora trae `skin = 2`
- **THEN** en ajustes queda resaltado el tercer círculo y tocarlo no cambia nada
