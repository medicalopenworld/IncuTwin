## ADDED Requirements

### Requirement: Gorros y mantas [test-unity]
El catálogo SHALL ofrecer, con su precio en corazones:
- Gorro de niño: Básico (de serie), Menta (15), Pollito (15), Nube (20).
- Gorro de niña: Básico (de serie), Lila (15), Melocotón (15), Menta (20).
- Manta: Básica (de serie), Crema (15), Menta (15), Lavanda (20), Melocotón (20).
Comprar un gorro de niño SHALL dejarlo elegido para los niños sin cambiar el de las niñas, y al
revés. La manta elegida SHALL valer para todos los bebés.

#### Scenario: Gorro de niño
- **WHEN** el saldo es 20 y se compra el gorro Menta de niño
- **THEN** el saldo pasa a 5, el gorro de niño es Menta y el de niña sigue siendo Básico

### Requirement: Navidad del 24 de diciembre al 6 de enero [test-unity]
El gorro de Papá Noel (10), el efecto Copos de nieve (10) y la guirnalda SHALL poder comprarse,
ponerse y verse solo del 24 de diciembre al 6 de enero, ambos incluidos (hora local). Fuera de esas
fechas, o sin hora, NO SHALL poder comprarse ni ponerse, y lo que ya se tiene NO SHALL verse. Papá
Noel y los copos se ponen y se quitan sin cambiar el gorro elegido. Lo comprado SHALL seguir en
años siguientes. La guirnalda SHALL seguir consiguiéndose con 10 días de diciembre acompañando.

#### Scenario: Antes del 24
- **WHEN** es 20 de diciembre, el saldo es 30 y se intenta comprar Papá Noel
- **THEN** no se compra, el saldo sigue en 30 y el motivo es «fuera de temporada»

#### Scenario: El 24
- **WHEN** es 24 de diciembre y el saldo es 30
- **THEN** se compra Papá Noel, el saldo pasa a 20 y Papá Noel queda puesto

#### Scenario: Después de Reyes
- **WHEN** es 7 de enero y Papá Noel estaba puesto
- **THEN** el bebé lleva el gorro elegido

### Requirement: Todo cuesta [test-unity]
Todo lo de la tienda SHALL tener un precio en corazones, salvo lo de serie (halo Cielo, fondo Blanco
roto, gorros y manta Básicos) y la guirnalda (se gana acompañando). Ningún sello ni logro SHALL
regalar artículos.

#### Scenario: Tras «Primera mano»
- **WHEN** se acaba de conseguir «Primera mano» con saldo 1 y se intenta comprar el gorro Lila (15)
- **THEN** no se compra y dice "Te faltan 14 corazones"

## MODIFIED Requirements

### Requirement: Persistencia [test-unity]
Saldo, total ganado, racha, abrazos guardados, sellos, pasaporte, desbloqueos y lo elegido SHALL
sobrevivir a reinicios y actualizaciones. Al actualizar desde el guardado anterior
(versión 1) SHALL conservarse todo lo que tenía, con lo nuevo de serie. El restablecimiento de
fábrica SHALL borrarlos.

#### Scenario: Actualizar desde la versión 1
- **WHEN** el guardado v1 tiene saldo 40, racha 9, Lavanda elegida, la guirnalda y el sello «Primera mano»
- **THEN** tras actualizar el saldo es 40, la racha 9, el halo Lavanda, la guirnalda sigue y los gorros y la manta son los Básicos

### Requirement: Demo aparte [test-unity]
Con la demo activa, saldo, sellos y pasaporte SHALL ser simulados: empiezan cada demo con 50
corazones (para poder comprar en la tienda), nivel Chispa, los sellos «Primera mano» y «Una semana» y
el pasaporte con CM. Al salir de la demo NO SHALL quedar nada de ello y lo de verdad NO SHALL haber
cambiado.

#### Scenario: Gesto en la demo
- **WHEN** en la demo se completa «Coge mi mano»
- **THEN** el saldo de la demo pasa de 50 a 51 y el saldo de verdad no cambia
