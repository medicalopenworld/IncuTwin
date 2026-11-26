## ADDED Requirements

### Requirement: Nombre de la IncuTwin [manual]
Ajustes SHALL tener «Nombre de tu IncuTwin», que enseña el nombre puesto o, si no hay, el número de
serie («ITW-0A1284»). Tocarlo SHALL abrir el teclado del panel: hasta 16 caracteres de los que caben
en las fuentes (letras, vocales con tilde, ñ, números y espacio); vacío vuelve al número de serie.
El asistente de primer arranque NO SHALL pedirlo. Debajo SHALL decir "Se verá en la IncuPray".

#### Scenario: Poner nombre
- **WHEN** se escribe "Lucía y Pablo" y se pulsa LISTO
- **THEN** Ajustes enseña "Lucía y Pablo" y el siguiente `status` lleva `"name":"Lucía y Pablo"`

#### Scenario: Borrar el nombre
- **WHEN** se borra todo y se pulsa LISTO
- **THEN** Ajustes enseña el número de serie y `status` deja de llevar `name`
