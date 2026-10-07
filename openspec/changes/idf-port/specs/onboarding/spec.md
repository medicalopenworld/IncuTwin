# onboarding

Asistente de primer arranque. Objetivo: que la familia no teclee nada en el panel; todo se hace
desde el móvil. Solo configura el WiFi: el emparejado con la incubadora lo hace Medical Open
World a distancia (ver `pairing`).

## ADDED Requirements

### Requirement: Arranque en onboarding [test-unity]
El panel SHALL entrar en el asistente cuando NVS `prov/done` no exista o sea falso, y SHALL
arrancar en la pantalla principal en caso contrario. Durante el asistente el botón BOOT no tiene
función y no se conecta al broker hasta el paso correspondiente.

#### Scenario: Primer arranque
- **WHEN** el panel arranca con la partición NVS recién flasheada de fábrica
- **THEN** muestra la pantalla de idioma

### Requirement: Paso 1, idioma [manual]
La primera pantalla SHALL mostrar la marca IncuTwin, el texto bilingüe "Elige tu idioma / Choose
your language" y dos botones "Español" / "English". La elección fija el idioma del resto del
asistente y del portal cautivo, y persiste.

#### Scenario: Elegir inglés
- **WHEN** se pulsa "English"
- **THEN** la siguiente pantalla aparece en inglés ("Connect your phone")

### Requirement: Paso 2, conecta tu móvil [manual]
El panel SHALL levantar el portal cautivo (ver `captive-portal`) y mostrar: título "Conecta tu
móvil", un QR `WIFI:T:WPA;S:<AP>;P:incutwin;;` de 130 px, los pasos "1. Escanea el código QR
2. Sigue los pasos en tu móvil" y, en pequeño, "Red: <AP> · Pass: incutwin". Si se viene de un
fallo de WiFi, SHALL mostrar además en rojo "No se pudo conectar. Comprueba la contraseña."

#### Scenario: QR funcional
- **WHEN** se escanea el QR con un móvil Android o iOS
- **THEN** el móvil se une a la red `IncuTwin-XXXX` y abre solo el portal

### Requirement: Paso 3, conectando a tu WiFi [manual]
Cuando el portal reciba el formulario, el panel SHALL mostrar un spinner con "Conectando a tu
WiFi..." e intentar conectar como estación con las credenciales recibidas. Si obtiene IP en
≤ 25 s SHALL apagar el portal y pasar al paso 4. Si no, SHALL volver al paso 2 con el aviso de
error, manteniendo el portal activo para reintentar.

#### Scenario: Contraseña incorrecta
- **WHEN** se envía una contraseña errónea para `in3wifi`
- **THEN** a los 25 s el panel vuelve al QR con el aviso en rojo y el móvil puede volver a enviar
  el formulario sin reconectarse al SoftAP

#### Scenario: Contraseña correcta
- **WHEN** se envía `in3wifi` / `12345678`
- **THEN** en < 25 s el panel pasa a "Conectando con el servidor..."

### Requirement: Paso 4, conectando con el servidor [manual]
El panel SHALL mostrar "Conectando con el servidor..." / "Connecting to the server..." mientras
intenta la primera sesión MQTT. SHALL pasar al paso 5 en cuanto conecte o, como máximo, a los
30 s. Si el panel no tiene credenciales MQTT de fábrica SHALL pasar al paso 5 de inmediato.

#### Scenario: Conexión al broker
- **WHEN** las credenciales de fábrica son válidas y hay internet
- **THEN** el paso 5 aparece en < 30 s y el pie de ajustes mostrará después "Servidor: Conectado"

#### Scenario: Sin internet
- **WHEN** la WiFi no tiene salida a internet
- **THEN** a los 30 s el asistente continúa igualmente al paso 5

### Requirement: Paso 5, listo [manual]
El panel SHALL mostrar "¡Listo!" / "All set!", el número de serie en texto y como QR (contenido
= el serie, sin URL), el texto "Medical Open World vinculará tu IncuTwin con una IncuNest. Verás
al bebé en cuanto esté lista." / "Medical Open World will link your IncuTwin to an IncuNest.
You'll see the baby as soon as it's ready." y un botón "Terminar" / "Finish". Al pulsarlo SHALL
marcar `prov/done = true` y reiniciar.

#### Scenario: Terminar
- **WHEN** se pulsa "Terminar"
- **THEN** el panel reinicia y arranca en la pantalla principal con la barra "Sin IncuNest
  vinculada" (si aún no se ha emparejado) o con el estado real (si el `cmd/pair` retenido ya
  existía)

### Requirement: Cambiar la WiFi desde la pantalla principal [manual]
Tocar el icono de cobertura WiFi de la pantalla principal SHALL abrir un diálogo "¿Cambiar la
red WiFi?" / "Change the WiFi network?" con botones "Cambiar" / "Change" y cerrar. Al confirmar,
el panel SHALL levantar el portal cautivo y mostrar la pantalla "Conecta tu móvil" (QR, red y
contraseña del SoftAP) con un botón "Cancelar" / "Cancel". Al recibir el formulario SHALL
desconectarse de la red actual, probar la nueva durante 25 s y, si obtiene IP, mostrar "¡Listo!"
y reiniciar; si falla, SHALL volver al QR con el aviso en rojo manteniendo el portal. Cancelar
SHALL reiniciar el panel (que vuelve a conectar con las credenciales guardadas). El
onboarding marcado como hecho NO SHALL cambiar.

#### Scenario: Cambio de red correcto
- **WHEN** se toca el icono WiFi, se confirma y el móvil envía otra red válida
- **THEN** el panel reinicia en < 40 s y arranca conectado a la nueva red con el mismo
  emparejamiento

#### Scenario: Cancelar
- **WHEN** se toca el icono WiFi, se confirma y luego se pulsa "Cancelar" sin enviar nada
- **THEN** el panel reinicia y vuelve a la red anterior; nada cambia en NVS

### Requirement: Reentrada tras factory reset [manual]
Tras un restablecimiento de fábrica el asistente SHALL comportarse igual que en el primer
arranque, conservando el serie y las credenciales MQTT.

#### Scenario: Reset y nuevo onboarding
- **WHEN** se hace factory reset y se completa el asistente con otra WiFi
- **THEN** el panel conecta a la nueva WiFi y al broker con el mismo client id de antes
