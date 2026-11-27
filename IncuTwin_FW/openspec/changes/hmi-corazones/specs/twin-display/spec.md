## ADDED Requirements

### Requirement: Saldo y nivel en la racha [manual]
La página de la racha SHALL enseñar, debajo de los siete corazones, el saldo junto a un corazón
("23"), el nivel ("Caricia") y los abrazos guardados (0 a 2 corazones pequeños en azul claro).
Fuera de Acompañar (sin página de racha) el saldo y el nivel están en la pantalla Corazones.

#### Scenario: Página de la racha
- **WHEN** el saldo es 23, el nivel Caricia y hay 1 abrazo guardado
- **THEN** la página dice "23" junto a un corazón, "Caricia" y enseña un abrazo guardado

### Requirement: Pantalla Corazones [manual]
La barra de navegación SHALL tener cinco botones: Inicio (casa), Notificaciones (sobre, con el punto
de no leídas), Corazones (corazón), Historial (pollito) y Ajustes (rueda). La pantalla Corazones
SHALL enseñar, en una lista con scroll: el saldo grande con el nivel y lo que falta para el
siguiente ("18 para Cosquillitas"); la racha (la semana de lunes a domingo, "N días seguidos" y los
abrazos guardados); los siete sellos (tocar uno abre "qué es y cómo se consigue"); el pasaporte
(todas las banderas, con un corazón dorado si alguno de sus bebés se fue a casa; vacío: "Dale la
mano a un bebé y su país aparecerá aquí"); y la tienda: «Una IncuNest con tu nombre» (Especial),
Halo, Fondo y, en su temporada o si ya se tiene, la Guirnalda. Cada opción SHALL enseñar su
muestra y su estado: «Elegido», «Usar», «Pedida» (el canje) o su precio con un corazón; la
guirnalda sin conseguir, los días de diciembre que se llevan ("3/10"). Tocar una bloqueada con
saldo SHALL preguntar antes de gastar; sin saldo SHALL decir "Te faltan N corazones". El saldo SHALL
verse arriba a la derecha aunque se baje por la lista. Inicio NO SHALL tener página de Logros.
Bajo la barra del nivel, si el siguiente nivel es Caricia o Cosquillitas, SHALL decir qué abre
("Caricia abre: acariciarle"); tocar la tarjeta del nivel SHALL abrir un diálogo con cómo se hace
lo que ya está abierto (siempre Manita; Caricia y Cosquillitas según el nivel).

#### Scenario: Desbloquear desde Corazones
- **WHEN** con 18 corazones se toca Lavanda y se confirma
- **THEN** Lavanda pasa a «Elegido», el saldo de arriba a 3 y el halo de Inicio es lavanda

#### Scenario: Ver un sello que falta
- **WHEN** se toca el sello «Un mes seguido» aún sin conseguir
- **THEN** sale un diálogo «Un mes seguido» con "30 días seguidos dándole la mano"

### Requirement: Latido en el icono del corazón [manual]
Tocar el icono del corazón SHALL hacer sonar el latido y latir el corazón del icono (crece un 25 %
y vuelve) al ritmo real del bebé durante 10 s; otro toque lo para. La burbuja SHALL decir "Este es
su latido" sin el número de pulsaciones (y, si late rápido, "El corazón de los bebés late muy
rápido").

#### Scenario: 140 por minuto
- **WHEN** el bebé tiene 140 lpm y se toca el corazón
- **THEN** el icono late 140 veces por minuto, suena el latido y la burbuja no enseña ningún número

### Requirement: Corazón que suma al saldo [manual]
Cuando el gesto da corazones, el corazón que vuela a la racha SHALL llevar el número que se gana
("+1", "+2" con el súper corazón) y, al llegar, el saldo de la caja de la racha SHALL subir.

#### Scenario: Súper corazón
- **WHEN** se llega a los 3 min de dedo
- **THEN** sale "+2" sobre la caja de la racha y el saldo sube 2

### Requirement: Aviso de sello o nivel [manual]
Un sello nuevo, la guirnalda o una subida a Abrazo o a Achuchón SHALL salir en la franja de aviso de
arriba ("¡Nuevo sello: Una semana!", "¡Subes a Abrazo!"), con las mismas reglas de cuándo puede
salir que los avisos del bebé. Las subidas a Caricia y a Cosquillitas NO van en la franja: salen en
la tarjeta del requisito «Afecto». NO SHALL guardarse en Notificaciones ni sumar al punto de no leídas. Tocar la
franja SHALL llevar a la pantalla Corazones.

#### Scenario: Sello al terminar el gesto
- **WHEN** el gesto concede «Una semana» y aún se ve el agradecimiento
- **THEN** la franja espera a que termine y entonces dice "¡Nuevo sello: Una semana!"

### Requirement: Afecto [manual]
Con Inicio a la vista y sin «Coge mi mano» en marcha, pasar el dedo por el bebé SHALL acariciarle
si el nivel es Caricia o más, y hacerle cosquillitas si es Cosquillitas o más y el dedo va rápido,
con la cara de cada una. Un dedo quieto sobre el bebé NO SHALL hacer nada. Tocar al bebé o
acariciarle NO SHALL sumar corazones, racha ni sellos: los corazones solo los da «Coge mi mano».
Al subir a Caricia o a Cosquillitas SHALL salir una tarjeta centrada («¡Subes a Caricia!») con lo
que se abre y un botón «Vale»; se queda hasta tocar el botón o la tarjeta y espera a que
«Coge mi mano» esté en reposo. Subir a Abrazo o a Achuchón mantiene la franja de
aviso. Quien ya tenía corazones al estrenar los niveles SHALL ver, en el primer arranque con Inicio
y sin demo, una sola vez la tarjeta «Tus corazones ya abren esto:» con lo que abre su nivel; con
la demo no sale ni se gasta.

#### Scenario: Caricia
- **WHEN** el nivel es Caricia y se pasa el dedo despacio por el bebé
- **THEN** el bebé pone la cara de caricia y el saldo no cambia

#### Scenario: Cosquillitas
- **WHEN** el nivel es Cosquillitas y se pasa el dedo rápido de un lado a otro por el bebé
- **THEN** el bebé pone la cara de cosquillitas y el saldo no cambia

#### Scenario: Dedo quieto
- **WHEN** el dedo se queda quieto sobre el bebé
- **THEN** no cambia la cara del bebé

#### Scenario: Tarjeta de subida de nivel
- **WHEN** el gesto lleva a 10 corazones ganados mientras se ve el agradecimiento de «Coge mi mano»
- **THEN** la tarjeta «¡Subes a Caricia!» espera a que termine y entonces sale con su texto y «Vale»; se
  queda hasta tocarla

#### Scenario: Aviso pendiente único
- **WHEN** se arranca con 10 corazones ganados de antes de los niveles de afecto y sin demo
- **THEN** sale la tarjeta «Tus corazones ya abren esto:» una vez; tras tocar «Vale» y reiniciar no vuelve

#### Scenario: No cuenta corazones
- **WHEN** se acaricia al bebé muchas veces
- **THEN** ni el saldo, ni los corazones ganados, ni la racha, ni los sellos cambian

### Requirement: Personalización aplicada [manual]
El halo en reposo SHALL pintarse con el color elegido y el fondo de Inicio y de sus páginas con el
fondo elegido. Calor, fototerapia y «un bebé te llama» SHALL seguir con sus colores de siempre. La
guirnalda, si está puesta y es temporada, SHALL colgar en lo alto de Inicio sin tapar la marca ni
el botón del país.

#### Scenario: Calor con halo lavanda
- **WHEN** el halo elegido es Lavanda y la incubadora empieza a calentar
- **THEN** el halo pasa al ámbar del calor; al estabilizarse vuelve al lavanda
