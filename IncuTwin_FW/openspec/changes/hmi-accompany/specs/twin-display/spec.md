## MODIFIED Requirements

### Requirement: Etiqueta del país [manual]
Si el estado trae `country`, en las vistas BEBÉ y CON SUS PAPÁS SHALL verse centrada sobre el
cuerpo del bebé (en la manta, entre la cara y el nombre) una etiqueta pequeña blanca con borde
azul, la bandera y el nombre del país en el idioma del panel ("Camerún" / "Cameroon"); al pulsarla
SHALL llevar a la página del país. Un código que el panel no conozca SHALL mostrarse tal cual. Sin
`country`, o mientras dura el gesto "Coge mi mano" (el dedo va justo ahí), NO SHALL verse.

#### Scenario: Camerún
- **WHEN** `show_baby` con `country = "CM"` y el panel en español
- **THEN** se ve la etiqueta "Camerún" sobre el cuerpo del bebé

#### Scenario: Código desconocido
- **WHEN** `show_baby` con `country = "XK"`
- **THEN** la etiqueta dice "XK"

#### Scenario: Durante el gesto
- **WHEN** se pulsa "Coge mi mano"
- **THEN** la etiqueta del país desaparece hasta que el gesto termina

## ADDED Requirements

### Requirement: Página del mapa con poco texto [manual]
La página del país SHALL mostrar solo el mapa, la etiqueta del país arriba y los km en una
pastilla sobre el centro de la línea que une tu IncuTwin con la IncuNest. La hora del país SHALL
ir en la barra de estado mientras se ve el mapa ("En Togo son las 17:43"), salvo avisos urgentes
(alarma, sin WiFi, sin credenciales, conexión perdida).

#### Scenario: Ver el mapa
- **WHEN** se desliza a la página del país de una IncuNest en Togo
- **THEN** sobre la línea pone "4.020 km" y la barra de estado "En Togo son las 17:43"

### Requirement: Volver al bebé desde el mapa [manual]
En la página del país, la etiqueta de arriba (bandera y nombre) SHALL llevar también el bebé en
pequeño y, al pulsarla, SHALL volver a la página del bebé.

#### Scenario: Pulsar la etiqueta del mapa
- **WHEN** en la página del país se pulsa la etiqueta "Togo" con el bebé
- **THEN** la pantalla se desliza a la página del bebé
