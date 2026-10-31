# captive-portal

Punto de acceso temporal + DNS + formulario web para que el móvil entregue al panel la red WiFi
de casa. Solo WiFi: no se piden datos personales.

## ADDED Requirements

### Requirement: Punto de acceso [manual]
Al iniciarse, el portal SHALL escanear las redes visibles (modo AP+STA), levantar un SoftAP WPA2
con SSID `IncuTwin-XXXX` (XXXX = últimos dos bytes de la MAC WiFi en hexadecimal mayúsculas) y
contraseña `incutwin`, con IP 192.168.4.1, y un servidor DNS que responda con esa IP a cualquier
nombre.

#### Scenario: Visible y accesible
- **WHEN** el portal está activo
- **THEN** un móvil ve la red `IncuTwin-XXXX`, se une con `incutwin` y recibe IP en 192.168.4.x

### Requirement: Detección de portal cautivo [manual]
Cualquier petición HTTP a una ruta distinta de `/` y `/save` SHALL responder `302` con
`Location: http://192.168.4.1/`, de modo que las sondas de Android (`/generate_204`), iOS
(`/hotspot-detect.html`) y Windows (`/connecttest.txt`) abran el portal automáticamente.

#### Scenario: Sonda de Android
- **WHEN** el móvil pide `http://connectivitycheck.gstatic.com/generate_204`
- **THEN** el DNS resuelve a 192.168.4.1 y la respuesta es 302 a `http://192.168.4.1/`, y el móvil
  muestra la notificación "iniciar sesión en la red" que abre el formulario

### Requirement: Formulario solo WiFi [manual]
`GET /` SHALL devolver una página HTML (estilo IncuTwin: fondo `#F4F3F0`, azul `#054E92`) en el
idioma elegido en el panel, con: un desplegable de hasta 15 SSID escaneados (sin duplicados ni
vacíos), un campo de contraseña y un botón "Conectar" / "Connect". No SHALL pedir nombre, email ni
consentimiento.

#### Scenario: Lista de redes
- **WHEN** hay 20 redes visibles, dos de ellas con SSID vacío
- **THEN** el desplegable muestra como máximo 15 redes y ninguna vacía

### Requirement: Envío y validación [test-unity]
El portal SHALL aceptar `POST /save` con `ssid` no vacío (≤ 32 bytes) y `pass` (≤ 63 bytes,
puede ser vacío para redes abiertas): guarda ambos en NVS `prov/ssid`, `prov/pass`, responde con
la página "¡Datos recibidos! Mira la pantalla del panel para continuar." y marca el portal como
ENVIADO. Si `ssid` está vacío SHALL devolver el formulario con "Revisa los campos marcados." /
"Please check the fields." sin guardar nada.

#### Scenario: Envío válido
- **WHEN** se envía `ssid=in3wifi&pass=12345678`
- **THEN** NVS contiene esos valores y el estado del portal es ENVIADO

#### Scenario: SSID vacío
- **WHEN** se envía `ssid=&pass=x`
- **THEN** la respuesta es el formulario con el mensaje de error y NVS no cambia

### Requirement: Parada limpia [manual]
Al pararse, el portal SHALL cerrar el servidor HTTP y el DNS, apagar el SoftAP y dejar el WiFi en
modo estación puro, liberando la memoria usada.

#### Scenario: Tras conectar
- **WHEN** el onboarding obtiene IP y para el portal
- **THEN** la red `IncuTwin-XXXX` desaparece en < 2 s y el heap libre vuelve a ± 4 KB del valor
  previo al portal

### Requirement: Reintento sin reconectar [manual]
Si el intento de WiFi falla, el portal SHALL seguir activo y aceptar un nuevo `POST /save` sin
que el móvil tenga que volver a unirse al SoftAP.

#### Scenario: Segundo intento
- **WHEN** falla la primera contraseña y el móvil envía otra
- **THEN** el segundo envío se acepta y el panel vuelve a intentar la conexión
