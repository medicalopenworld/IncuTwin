# wifi-connectivity

Estación WiFi del panel, reconexión y hora de red.

## ADDED Requirements

### Requirement: Conexión con credenciales guardadas [manual]
Con onboarding completado, el panel SHALL conectar como estación a `prov/ssid` con `prov/pass` al
arrancar, publicar el evento "WiFi conectada" con la IP al obtenerla y "WiFi desconectada" al
perderla. La IP SHALL imprimirse en el log.

#### Scenario: Arranque con WiFi disponible
- **WHEN** el panel arranca y `in3wifi` está al alcance
- **THEN** obtiene IP en < 10 s, el log muestra `wifi: ip 192.168.x.y` y el icono WiFi pasa de
  tachado a barras

### Requirement: Reconexión automática [manual]
Al perder la conexión, el panel SHALL reintentar indefinidamente con espera creciente 1, 2, 4,
8, 16, 30 s (tope 30 s) entre intentos, reiniciando la espera a 1 s tras conectar. Nunca SHALL
reiniciar el dispositivo por falta de WiFi.

#### Scenario: Router apagado 5 minutos
- **WHEN** el router se apaga 5 min y se vuelve a encender
- **THEN** el panel recupera IP en < 45 s desde que el router vuelve, sin reiniciarse, y el
  estado retenido del broker reaparece en pantalla

### Requirement: Cobertura medida [test-unity]
El panel SHALL muestrear el RSSI cada 1 s mientras está conectado y clasificarlo en 4 niveles:
≥ −55 dBm → 3; ≥ −67 → 2; ≥ −78 → 1; menor → 0.

#### Scenario: Umbrales
- **WHEN** el RSSI es −55, −56, −67, −68, −78, −79 dBm
- **THEN** los niveles son 3, 2, 2, 1, 1, 0 respectivamente

### Requirement: Hora por SNTP [test-unity]
Al obtener IP, el panel SHALL sincronizar la hora con `pool.ntp.org` (configurable) y considerar
la hora válida solo si el año es ≥ 2025. Mientras no sea válida, los campos `ts` que publique
SHALL valer 0.

#### Scenario: Hora válida
- **WHEN** pasan 30 s desde la IP con internet
- **THEN** `time(NULL)` devuelve una fecha de 2025 o posterior y el siguiente `status` lleva `ts`
  en segundos Unix

### Requirement: Credenciales inválidas no bloquean [manual]
Si las credenciales guardadas ya no sirven (la familia cambió de router), el panel SHALL seguir
reintentando y mostrar "Sin conexión WiFi"; la única salida es el restablecimiento de fábrica
desde ajustes, que SHALL seguir siendo accesible.

#### Scenario: Red desaparecida
- **WHEN** la red guardada ya no existe
- **THEN** la pantalla principal funciona (ajustes, demo) con la barra "Sin conexión WiFi" en rojo
