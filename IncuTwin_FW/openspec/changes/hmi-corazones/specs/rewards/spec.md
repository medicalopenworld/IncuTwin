## ADDED Requirements

### Requirement: Saldo de corazones [test-unity]
La IncuTwin SHALL llevar un saldo de corazones que solo se gana con lo que hace quien acompaña,
nunca con lo que le pasa al bebé (ingreso, tratamientos, salida):
- +1 por el primer «Coge mi mano» completado de cada día (hora local de España).
- +2 por el primer súper corazón del día (3 min de dedo en el mismo gesto, modo Acompañar).
- +1 cada vez que la racha llega a un múltiplo de 7 días (7, 14, 21...).
Fuera de la IncuPray, un día SHALL sumar como mucho 4 corazones. Sin hora en el reloj (SNTP
pendiente) los gestos NO SHALL sumar, porque no se sabe qué día es.

#### Scenario: Primer gesto del día
- **WHEN** el saldo es 10 y hoy aún no se ha completado «Coge mi mano»
- **THEN** al completarlo el saldo pasa a 11; un segundo gesto el mismo día lo deja en 11

#### Scenario: Súper corazón
- **WHEN** en modo Acompañar se completa el gesto y se mantiene el dedo hasta los 3 min
- **THEN** el saldo sube 1 al completar y 2 más al llegar a los 3 min (3 en total ese día)

#### Scenario: Una semana seguida
- **WHEN** hoy se completa el gesto y es el 7.º día seguido de racha
- **THEN** el saldo sube 1 por el gesto y 1 por la semana

#### Scenario: Sin hora
- **WHEN** el panel aún no tiene hora y se completa el gesto
- **THEN** el saldo no cambia

### Requirement: Racha larga y comodín [test-unity]
La racha SHALL contar los días seguidos con «Coge mi mano» completado, sin tope de 7. Un día sin
bebés (modo Acompañar, día de descanso) NO SHALL romperla ni sumarla. Cada vez que la racha llega
a un múltiplo de 7 SHALL guardarse un «abrazo guardado» (máximo 2). Un día sin gesto que no sea
de descanso SHALL gastar un abrazo guardado si lo hay (la racha sigue, sin sumar ese día); si no
lo hay, la racha SHALL volver a empezar en el siguiente gesto, sin ningún mensaje de pérdida.

#### Scenario: Un día sin acompañar con abrazo guardado
- **WHEN** la racha es de 9 días con 1 abrazo guardado, ayer no hubo gesto ni fue día de descanso
  y hoy se completa el gesto
- **THEN** la racha pasa a 10 y quedan 0 abrazos guardados

#### Scenario: Sin abrazo guardado
- **WHEN** la racha es de 5, no hay abrazos guardados, ayer no hubo gesto y hoy se completa
- **THEN** la racha pasa a 1

#### Scenario: Día sin bebés
- **WHEN** la racha es de 4 y ayer fue día de descanso
- **THEN** hoy, con gesto, la racha pasa a 5 y no se gasta ningún abrazo guardado

### Requirement: Niveles [test-unity]
El nivel SHALL salir de los corazones ganados en total (gastar no lo baja): Manita (0),
Caricia (10), Cosquillitas (30), Abrazo (75) y Achuchón (150). ES/EN: Manita/Little hand,
Caricia/Caress, Cosquillitas/Tickles, Abrazo/Hug, Achuchón/Bear hug.

#### Scenario: Subir de nivel
- **WHEN** se llevan 9 corazones ganados en total y se gana 1
- **THEN** el nivel pasa de Manita a Caricia

#### Scenario: Gastar no baja
- **WHEN** en nivel Caricia con 10 ganados en total se gastan 5
- **THEN** el nivel sigue siendo Caricia

### Requirement: Sellos [test-unity]
La IncuTwin SHALL conceder cada sello una sola vez, al cumplirse:
- «Primera mano»: primer «Coge mi mano» completado.
- «Una semana»: racha de 7 días.
- «Un mes seguido»: racha de 30 días.
- «30 días»: 30 días con gesto en total (no hace falta seguidos).
- «Súper corazón»: primer súper corazón.
- «Tres bebés»: has dado la mano a 3 bebés distintos (estancias distintas).
- «A casa»: un bebé al que diste la mano se fue a casa (su IncuNest pasa a `baby: "home"`
  durante su estancia). Una salida neutra NO SHALL conceder nada.
NO SHALL haber sellos que dependan de la hora del día.

#### Scenario: Tres bebés
- **WHEN** se ha dado la mano a dos bebés de estancias distintas y hoy se completa el gesto con un
  tercero
- **THEN** se concede «Tres bebés» y sale su aviso

#### Scenario: Salida sin «a casa»
- **WHEN** el bebé al que diste la mano sale de la IncuNest sin pasar por `home`
- **THEN** no se concede ningún sello ni corazón

### Requirement: Pasaporte [test-unity]
El pasaporte SHALL guardar cada país (ISO alfa-2 del estado de la IncuNest) en el que se ha
completado «Coge mi mano», una vez por país, en el orden en que se consiguieron. Un país SHALL
llevar una estrella si algún bebé al que diste la mano allí se fue a casa. Sin país en el estado
NO SHALL añadirse nada.

#### Scenario: País nuevo
- **WHEN** el pasaporte tiene TG y se completa el gesto con un bebé de una IncuNest de CM
- **THEN** el pasaporte pasa a TG, CM

#### Scenario: Estrella
- **WHEN** un bebé de TG al que diste la mano pasa a `home`
- **THEN** TG lleva estrella

### Requirement: Personalizar con corazones [test-unity]
El catálogo SHALL ofrecer, con su precio en corazones:
- Halo en reposo: Cielo (de serie), Lavanda (15), Menta (15), Melocotón (20), Lila (30).
- Fondo de Inicio: Blanco roto (de serie), Crema (10), Rosa (20), Verde agua (20).
Desbloquear SHALL restar el precio del saldo una sola vez y dejarlo elegido; lo desbloqueado SHALL
poder elegirse después sin coste. Sin saldo suficiente NO SHALL desbloquearse. Solo cambia el halo
en reposo: los de calor, fototerapia y «un bebé te llama» SHALL mantener sus colores.

#### Scenario: Desbloquear
- **WHEN** el saldo es 18 y se desbloquea Lavanda (15)
- **THEN** el saldo pasa a 3 y el halo en reposo es lavanda

#### Scenario: Sin saldo
- **WHEN** el saldo es 12 y se intenta desbloquear Melocotón (20)
- **THEN** no se desbloquea, el saldo sigue en 12 y se dice "Te faltan 8 corazones"

### Requirement: Una IncuNest con tu nombre [test-unity]
La tienda SHALL ofrecer «Una IncuNest con tu nombre» por 365 corazones. Canjearlo SHALL restar el
precio una sola vez, dejarlo como pedido (no se puede volver a canjear) y no cambiar lo elegido.
El panel SHALL avisar del canje al broker (`incutwin/<id>/redeem`) hasta que el envío salga una vez,
también tras reiniciar o sin conexión en el momento. En la demo NO SHALL poder canjearse. Lo cumple
Medical Open World fuera del panel (contacta con el dueño de la suscripción para elegir el nombre).

#### Scenario: Canjear
- **WHEN** el saldo es 400 y se canjea la IncuNest con tu nombre
- **THEN** el saldo pasa a 35, la opción dice «Pedida» y sale `incutwin/<id>/redeem`
  `{"redeem":"named_incunest","hearts":365,"ts":...}`

#### Scenario: Sin conexión al canjear
- **WHEN** se canjea con el broker caído y luego vuelve la conexión
- **THEN** el aviso de canje sale al reconectar, una sola vez

### Requirement: Guirnalda de diciembre [test-unity]
En diciembre, acompañar (gesto completado) 10 días distintos del mes SHALL desbloquear gratis la
guirnalda de temporada, que se puede poner y quitar en Personalizar y solo se ve del 1 de
diciembre al 6 de enero. Lo conseguido un año SHALL seguir para los siguientes.

#### Scenario: Décimo día de diciembre
- **WHEN** es 14 de diciembre, se llevan 9 días con gesto este mes y hoy se completa el gesto
- **THEN** se desbloquea la guirnalda y sale su aviso

### Requirement: Demo aparte [test-unity]
Con la demo activa, saldo, sellos y pasaporte SHALL ser simulados: empiezan cada demo con 12
corazones, nivel Caricia, los sellos «Primera mano» y «Una semana» y el pasaporte con CM; los gestos
de la demo suman sobre eso. Al salir de la demo NO SHALL quedar nada de ello y lo de verdad NO SHALL
haber cambiado.

#### Scenario: Gesto en la demo
- **WHEN** en la demo se completa «Coge mi mano»
- **THEN** el saldo de la demo pasa de 12 a 13 y el saldo de verdad no cambia

### Requirement: Persistencia [test-unity]
Saldo, total ganado, racha, abrazos guardados, sellos, pasaporte, desbloqueos y lo elegido SHALL
sobrevivir a reinicios y actualizaciones (se guardan en cuanto cambian). El restablecimiento de
fábrica SHALL borrarlos.

#### Scenario: Reinicio
- **WHEN** el saldo es 40 con Lavanda elegida y el panel se reinicia
- **THEN** arranca con 40 corazones y el halo lavanda
