## MODIFIED Requirements

### Requirement: Descarga y verificación [manual]
La descarga SHALL empezar cuando se abre la ventana de la actualización (requisito «Cuándo sale la
ventana»). El panel SHALL descargar la URL por HTTPS (certificado verificado con el bundle,
redirecciones ≤ 3, timeout de recepción 10 s; la build `dev` puede no comprobar el certificado),
escribiendo el flujo en la partición OTA inactiva mientras calcula el sha256 acumulado. Al
terminar: si el sha256 no coincide con el del comando, SHALL abortar sin tocar la partición de
arranque; si coincide y la imagen es válida para el bootloader, SHALL fijarla como partición de
arranque y reiniciar ~1,5 s después. El progreso SHALL registrarse cada 10 %. Un error de red SHALL
abortar limpiamente y dejar el panel funcionando con el firmware actual.

#### Scenario: Actualización correcta
- **WHEN** llega un `cmd/ota` válido con el hash correcto de un binario de 3 MB y la ventana se abre
- **THEN** la barra llega al 100 %, sale «¡Lista! Reiniciando...», el panel reinicia y el siguiente
  `status` lleva el `fw` nuevo

#### Scenario: Hash incorrecto
- **WHEN** el `sha256` del comando no es el del binario
- **THEN** el log muestra `ota: sha256 mismatch`, la ventana dice «No se ha podido actualizar» y a
  los 4 s se cierra; no hay reinicio

#### Scenario: Servidor caído a mitad
- **WHEN** el servidor corta la conexión al 40 %
- **THEN** el log muestra el error, la ventana dice «No se ha podido actualizar» y se cierra, y un
  `cmd/ota` posterior vuelve a intentarlo desde cero

### Requirement: Pantalla durante la OTA [manual]
Mientras dura la descarga SHALL verse una ventana modal encima de todo (fondo oscurecido que tapa
los toques; sin barra de navegación ni franjas por encima) con «Actualizando tu IncuTwin»,
«Tardará un minuto. No la desenchufes.», una barra de progreso y el porcentaje. Al acabar SHALL
decir «¡Lista! Reiniciando...». Si falla, «No se ha podido actualizar. Tu IncuTwin sigue como
estaba.» durante 4 s y se cierra.

#### Scenario: Progreso
- **WHEN** la descarga va por la mitad
- **THEN** la barra está a media altura y el porcentaje dice ~50 %

## ADDED Requirements

### Requirement: Cuándo sale la ventana [manual]
Con un `cmd/ota` válido aceptado (o vencido el retardo de flota), la ventana SHALL abrirse:
- en seguida, si el panel arrancó hace menos de 5 min y nadie ha tocado la pantalla desde entonces;
- si no, cuando lleve 2 min sin que se toque la pantalla y sin «Coge mi mano» a medias;
- si han pasado 6 h, en cuanto no haya «Coge mi mano» a medias.
Mientras espera, la pantalla NO SHALL cambiar y un `cmd/ota` nuevo SHALL ignorarse con log.

#### Scenario: Recién arrancado
- **WHEN** el panel arranca, conecta y recibe un `cmd/ota` sin que nadie haya tocado la pantalla
- **THEN** la ventana sale en seguida y empieza la descarga

#### Scenario: Alguien la está usando
- **WHEN** llega un `cmd/ota` mientras alguien mantiene el dedo en «Coge mi mano»
- **THEN** el gesto sigue sin cortes y la ventana sale 2 min después de la última vez que se tocó

### Requirement: Firmware objetivo [test-unity]
El panel SHALL aceptar en `incutwin/<id>/cmd/fw` e `incutwin/all/cmd/fw` (retenidos) un firmware
objetivo `{"version","url","sha256"}` (url y sha256 con las mismas reglas que `cmd/ota`; versión de
1 a 31 caracteres). El objetivo del panel SHALL mandar sobre el de la flota. Si la versión del
objetivo es mayor que la que corre (comparando número a número; lo que va tras `-` o `+` no
cuenta), SHALL hacer la OTA: la del panel en seguida y la de la flota con el retardo aleatorio de
flota. Con la misma versión o una menor NO SHALL hacer nada (nunca baja). Un payload vacío SHALL
borrar ese objetivo. Un binario descartado (sha256 distinto, imagen inválida, 3 descargas sin
acabar o rollback tras instalarlo) NO SHALL volver a intentarse aunque el objetivo siga retenido.

#### Scenario: Panel por detrás
- **WHEN** el panel lleva 2.0.0 y al conectar recibe el objetivo de flota 2.1.0
- **THEN** espera su retardo aleatorio y hace la OTA a 2.1.0

#### Scenario: Ya al día
- **WHEN** el panel lleva 2.1.0 y recibe el objetivo 2.1.0 (o 2.0.0)
- **THEN** no hace nada

#### Scenario: Piloto
- **WHEN** hay objetivo de flota 2.0.0 y objetivo del panel 2.1.0, y el panel lleva 2.0.0
- **THEN** el panel hace la OTA a 2.1.0 en seguida

#### Scenario: Firmware roto
- **WHEN** el 2.1.0 se instala pero no llega a conectar y el bootloader vuelve al 2.0.0
- **THEN** al conectar, el objetivo retenido 2.1.0 se ignora ("ese binario ya fallo")

### Requirement: Reintento tras un corte [manual]
Al aceptar un `cmd/ota` el panel SHALL guardar la OTA (URL, sha256, intentos) en NVS. Si la
descarga se corta (luz, red), SHALL volver a intentarla al reconectar con el broker, también tras
reiniciar, hasta 3 descargas en total. Un sha256 distinto o una imagen inválida NO SHALL
reintentarse. Instalada, el siguiente arranque SHALL borrarla (también si luego hay rollback: no se
reintenta un firmware que no arranca).

#### Scenario: Se va la luz al 50 %
- **WHEN** la descarga va por el 50 % y se corta la alimentación
- **THEN** el panel arranca con su firmware de siempre y, al conectar, vuelve a abrir la ventana
  («intento 2 de 3»), descarga, reinicia y queda con el firmware nuevo
