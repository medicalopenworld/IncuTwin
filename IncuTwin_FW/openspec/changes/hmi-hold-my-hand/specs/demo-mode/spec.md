## MODIFIED Requirements

### Requirement: Escenarios [manual]
La pulsación corta en demo SHALL pasar al siguiente escenario, en bucle, en este orden:

| # | id | Lo que se ve |
|---|---|---|
| 1 | sleep | bebé durmiendo, 120 lpm, calor estable |
| 2 | awake | bebé despierto, 140 lpm |
| 3 | heating | calentando (halo ámbar), 130 lpm |
| 4 | alarm | ¡Alarma! (halo rojo rápido), 180 lpm |
| 5 | photo | fototerapia (halo azul), 130 lpm |
| 6 | parents | con sus papás |
| 7 | home | ¡ya está en casa! |
| 8 | empty | IncuNest sin bebé |
| 9 | off | IncuNest apagada |
| 10 | unlinked | sin IncuNest vinculada |

Cada escenario SHALL producir las mismas transiciones sonoras que si llegara por red (entrar en
`sleep` desde `unlinked` suena "Bebé detectado"; entrar en `parents` suena la fanfarria).
Fuera de demo la pulsación corta no hace nada. Todos los escenarios SHALL traer el país
Camerún (`country = "CM"`).

#### Scenario: Ciclo completo
- **WHEN** se pulsa BOOT brevemente 10 veces desde el escenario 1
- **THEN** se recorren los 10 escenarios y se vuelve a "sleep", que vuelve a sonar

#### Scenario: Corta fuera de demo
- **WHEN** se pulsa BOOT brevemente sin demo activa
- **THEN** no pasa nada

## ADDED Requirements

### Requirement: Probar el tono de piel en demo [manual]
Con la demo activa, los 5 círculos de tono de piel de ajustes SHALL ser pulsables: tocar uno SHALL
aplicar ese tono al bebé y a la imagen con sus papás de la demo al momento. El tono probado se
mantiene al cambiar de escenario, no se guarda y al salir de la demo vuelve el tono real.

#### Scenario: Probar piel oscura
- **WHEN** en demo se toca el cuarto círculo y se pasa al escenario "parents"
- **THEN** el bebé se ve con el tono 3 y la imagen con sus papás es la de la familia de piel oscura

#### Scenario: Salir de la demo
- **WHEN** tras probar un tono se sale de la demo
- **THEN** vuelve el tono que dice la incubadora

### Requirement: Páginas deslizables en demo [manual]
Con la demo activa, la pantalla principal SHALL permitir deslizar entre tres páginas (bebé; país,
hospital y hora local; logros), con los datos simulados de Camerún que fija `twin-display`
("Páginas de la demo"). Al salir de la demo SHALL volver a la página 1 y los logros de la demo se
ponen a cero.

#### Scenario: Salir de la demo en la página 3
- **WHEN** en la página 3 se mantiene BOOT 2 s
- **THEN** sale de la demo y se ve la página 1, sin puntos de página
