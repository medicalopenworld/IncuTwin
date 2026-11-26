## ADDED Requirements

### Requirement: Saldo y nivel en la racha [manual]
La página de la racha SHALL enseñar, debajo de los siete corazones, el saldo junto a un corazón
("23"), el nivel ("Calorcito") y los abrazos guardados (0 a 2 corazones pequeños en azul claro).
Fuera de Acompañar (sin página de racha) el saldo y el nivel están en la pantalla Corazones.

#### Scenario: Página de la racha
- **WHEN** el saldo es 23, el nivel Calorcito y hay 1 abrazo guardado
- **THEN** la página dice "23" junto a un corazón, "Calorcito" y enseña un abrazo guardado

### Requirement: Pantalla Corazones [manual]
La barra de navegación SHALL tener cinco botones: Inicio (casa), Notificaciones (sobre, con el punto
de no leídas), Corazones (corazón), Historial (pollito) y Ajustes (rueda). La pantalla Corazones
SHALL enseñar, en una lista con scroll: el saldo grande con el nivel y lo que falta para el
siguiente ("12 para Calorcito"); la racha (la semana de lunes a domingo, "N días seguidos" y los
abrazos guardados); los siete sellos (tocar uno abre "qué es y cómo se consigue"); el pasaporte
(todas las banderas, con un corazón dorado si alguno de sus bebés se fue a casa; vacío: "Dale la
mano a un bebé y su país aparecerá aquí"); y la tienda: «Una IncuNest con tu nombre» (Especial),
Halo, Fondo y, en su temporada o si ya se tiene, la Guirnalda. Cada opción SHALL enseñar su
muestra y su estado: «Elegido», «Usar», «Pedida» (el canje) o su precio con un corazón; la
guirnalda sin conseguir, los días de diciembre que se llevan ("3/10"). Tocar una bloqueada con
saldo SHALL preguntar antes de gastar; sin saldo SHALL decir "Te faltan N corazones". El saldo SHALL
verse arriba a la derecha aunque se baje por la lista. Inicio NO SHALL tener página de Logros.

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
Un sello nuevo, una subida de nivel o la guirnalda SHALL salir en la franja de aviso de arriba
("¡Nuevo sello: Una semana!", "¡Subes a Abrazo!"), con las mismas reglas de cuándo puede salir que
los avisos del bebé. NO SHALL guardarse en Notificaciones ni sumar al punto de no leídas. Tocar la
franja SHALL llevar a la pantalla Corazones.

#### Scenario: Sello al terminar el gesto
- **WHEN** el gesto concede «Una semana» y aún se ve el agradecimiento
- **THEN** la franja espera a que termine y entonces dice "¡Nuevo sello: Una semana!"

### Requirement: Personalización aplicada [manual]
El halo en reposo SHALL pintarse con el color elegido y el fondo de Inicio y de sus páginas con el
fondo elegido. Calor, fototerapia y «un bebé te llama» SHALL seguir con sus colores de siempre. La
guirnalda, si está puesta y es temporada, SHALL colgar en lo alto de Inicio sin tapar la marca ni
el botón del país.

#### Scenario: Calor con halo lavanda
- **WHEN** el halo elegido es Lavanda y la incubadora empieza a calentar
- **THEN** el halo pasa al ámbar del calor; al estabilizarse vuelve al lavanda
