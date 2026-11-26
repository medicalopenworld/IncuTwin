## ADDED Requirements

### Requirement: Gorro y manta en el bebé [manual]
El bebé de Inicio SHALL llevar el gorro elegido para su sexo y la manta elegida, cambiando solo los
colores del gorro y de la manta (la cara, el contorno, el pompón y el lazo no cambian), con cualquier
tono de piel. Del 24-dic al 6-ene con Papá Noel puesto SHALL llevar el gorro de Papá Noel encima.

#### Scenario: Niña con gorro Lila y manta Crema
- **WHEN** el bebé es niña, el gorro de niña es Lila y la manta Crema
- **THEN** el gorro es lila con su lazo, la manta crema y la capucha de la manta crema

### Requirement: Copos de nieve [manual]
Con los copos puestos, del 24-dic al 6-ene, SHALL caer despacio alrededor del bebé copos blancos
sin tocarlo, sin parpadeos. NO SHALL verse mientras un bebé te llama ni durante «Coge mi mano».

#### Scenario: 25 de diciembre
- **WHEN** es 25 de diciembre y los copos están puestos
- **THEN** en Inicio caen copos alrededor del bebé

### Requirement: Pantalla Tienda [manual]
Lo primero de Corazones SHALL ser la tarjeta «Tienda» y, debajo, la de «Una IncuNest con tu
nombre». «Tienda» SHALL abrir una pantalla con el saldo arriba a la derecha y las pestañas Gorros,
Mantas, Fondos y Efectos, todas visibles a la vez. Gorros SHALL tener un conmutador Niño / Niña. Caben
dos filas de tres tarjetas sin que se monten los textos; cada tarjeta SHALL enseñar su muestra
(el bebé con esa ropa puesta en gorros y mantas), su nombre y su estado: «Elegido», «Usar», el precio
con un corazón o «Desde el 24» (Navidad fuera de temporada). Tocar un gorro, una
manta o Papá Noel SHALL abrir Probar: el bebé con eso puesto y un botón «Comprar por N», «Usar»,
«Elegido», «Desde el 24 de diciembre» (inactivo) o «Te faltan N corazones» (inactivo). Comprar
SHALL preguntar antes de gastar.

#### Scenario: Probar y comprar
- **WHEN** con 23 corazones se toca el gorro Menta de niño y luego «Comprar por 15» y se confirma
- **THEN** el saldo pasa a 8, el botón dice «Elegido» y el bebé de Inicio lleva el gorro Menta

#### Scenario: Navidad antes del 24
- **WHEN** es 10 de diciembre y se toca Papá Noel
- **THEN** se ve el bebé con el gorro de Papá Noel y el botón dice «Desde el 24 de diciembre», inactivo

### Requirement: Guirnalda desde el 24 [manual]
La guirnalda SHALL verse en Inicio solo del 24-dic al 6-ene si está conseguida y puesta.

#### Scenario: 15 de diciembre con la guirnalda conseguida
- **WHEN** es 15 de diciembre y la guirnalda está conseguida
- **THEN** no se ve en Inicio y en la tienda dice «Desde el 24»
