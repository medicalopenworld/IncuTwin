# ota-update

Actualización de firmware por HTTPS disparada por `cmd/ota`, con verificación sha256 antes de
activar y rollback del bootloader si el nuevo firmware no llega a conectar
(`docs/BROKER-MQTT-CONTEXT.md` §10).

## ADDED Requirements

### Requirement: Tabla de particiones [test-unity]
La flash de 16 MB SHALL dividirse así, sin partición `factory`:

| Nombre | Tipo | Subtipo | Offset | Tamaño |
|---|---|---|---|---|
| nvs | data | nvs | 0x9000 | 0x5000 |
| otadata | data | ota | 0xE000 | 0x2000 |
| ota_0 | app | ota_0 | 0x10000 | 0x400000 |
| ota_1 | app | ota_1 | 0x410000 | 0x400000 |
| storage | data | spiffs | 0x810000 | 0x7D0000 |
| coredump | data | coredump | 0xFE0000 | 0x20000 |

El binario de la aplicación SHALL caber en 4 MB con ≥ 1 MB de margen.

#### Scenario: Build dentro del límite
- **WHEN** se compila la versión de release
- **THEN** `idf.py size` muestra un binario ≤ 3 MB y la tabla generada coincide con la anterior

### Requirement: Validación del comando [test-unity]
`cmd/ota` SHALL aceptarse solo si el payload es `{"url":"<url>","sha256":"<hex>"}` con `url` de
esquema `https`, host que termine en `.medicalopenworld.org` (sufijo configurable en Kconfig) y
≤ 256 bytes, y `sha256` de exactamente 64 caracteres hexadecimales. Cualquier otra cosa SHALL
rechazarse con log WARN. Una build de desarrollo PUEDE desactivar la restricción de host con
`CONFIG_INCUTWIN_OTA_ALLOW_ANY_HOST`, que SHALL quedar deshabilitada por defecto y hacer que el
log de arranque avise en mayúsculas.

#### Scenario: URL de otro dominio
- **WHEN** llega `{"url":"https://evil.example.com/fw.bin","sha256":"<64 hex>"}`
- **THEN** se rechaza y nada se descarga

#### Scenario: HTTP en claro
- **WHEN** llega `{"url":"http://fw.medicalopenworld.org/x.bin", ...}`
- **THEN** se rechaza

#### Scenario: Hash mal formado
- **WHEN** `sha256` tiene 63 caracteres
- **THEN** se rechaza

### Requirement: Un trabajo a la vez y retardo de flota [test-unity]
Mientras una OTA esté en curso, nuevos `cmd/ota` SHALL ignorarse con log. Un `cmd/ota` recibido
por `incutwin/all/cmd/ota` SHALL esperar un retardo aleatorio uniforme entre 0 y 3600 s
(configurable) antes de empezar la descarga; si durante la espera llega un `cmd/ota` unicast,
este SHALL sustituir al pendiente y ejecutarse sin retardo.

#### Scenario: Flota
- **WHEN** 3 paneles reciben `incutwin/all/cmd/ota` a la vez
- **THEN** cada uno registra un retardo distinto en [0, 3600] s y empieza la descarga al vencer

#### Scenario: Unicast prioritario
- **WHEN** un panel espera su retardo de flota y recibe un `cmd/ota` unicast
- **THEN** descarga de inmediato el unicast y descarta el de flota

### Requirement: Descarga y verificación [manual]
El panel SHALL descargar la URL por HTTPS (certificado verificado con el bundle, redirecciones
≤ 3, timeout de recepción 10 s), escribiendo el flujo en la partición OTA inactiva mientras
calcula el sha256 acumulado. Al terminar: si el sha256 no coincide con el del comando, SHALL
abortar sin tocar la partición de arranque; si coincide y la imagen es válida para el
bootloader, SHALL fijarla como partición de arranque y reiniciar 1 s después. El progreso SHALL
registrarse cada 10 %. Un error de red SHALL abortar limpiamente y dejar el panel funcionando con
el firmware actual.

#### Scenario: Actualización correcta
- **WHEN** llega un `cmd/ota` válido con el hash correcto de un binario de 1,8 MB
- **THEN** la descarga termina en < 60 s sobre WiFi doméstica, el panel reinicia y el siguiente
  `status` lleva el `fw` nuevo

#### Scenario: Hash incorrecto
- **WHEN** el `sha256` del comando no es el del binario
- **THEN** el log muestra `ota: sha256 mismatch`, no hay reinicio y el panel sigue mostrando el
  estado del bebé

#### Scenario: Servidor caído a mitad
- **WHEN** el servidor corta la conexión al 40 %
- **THEN** el log muestra el error, la UI no se ve afectada y un `cmd/ota` posterior vuelve a
  intentarlo desde cero

### Requirement: Pantalla durante la OTA [manual]
La descarga NO SHALL alterar la pantalla principal (la familia no tiene que saber nada). El pie
de ajustes PUEDE mostrar "Actualizando... n %".

#### Scenario: Bebé visible durante la descarga
- **WHEN** hay OTA en curso con el bebé en pantalla
- **THEN** el bebé sigue respirando y latiendo con fluidez hasta el reinicio

### Requirement: Rollback del bootloader [manual]
El bootloader SHALL compilarse con rollback habilitado. Tras una OTA, el firmware nuevo arranca
pendiente de verificación y solo se marca válido al conectar al broker (ver `broker-link`). Si
se reinicia (corte de alimentación, pánico o el watchdog de validación) sin haberse marcado
válido, el bootloader SHALL arrancar el firmware anterior y marcar el nuevo como inválido.

#### Scenario: Firmware que se cuelga
- **WHEN** se instala por OTA una build que entra en pánico a los 5 s de arrancar
- **THEN** tras el reinicio automático el panel arranca con el firmware anterior y el log muestra
  el slot anterior

#### Scenario: Flasheo de fábrica no se revierte
- **WHEN** se flashea el firmware por USB con `idf.py flash` y el panel no tiene internet
- **THEN** el firmware sigue arrancando indefinidamente (no está pendiente de verificación)

### Requirement: Versión reportada [test-unity]
La versión del firmware (`fw` en `status`, pie de ajustes, log de arranque) SHALL provenir de un
único sitio (la descripción de la aplicación, `PROJECT_VER`) y seguir semver `MAJOR.MINOR.PATCH`.

#### Scenario: Una sola fuente
- **WHEN** se cambia la versión del proyecto a 2.1.0 y se compila
- **THEN** el log de arranque, el pie de ajustes y `status` dicen 2.1.0
