# board-bringup

Arranque del hardware del CrowPanel Advance 2.8" (ESP32-S3-WROOM-1-N16R8) en orientación
vertical con el USB arriba. Es la base de todo lo demás: si esto no está bien, nada de lo que
pinte la UI es fiable.

## ADDED Requirements

### Requirement: Pantalla en vertical, USB arriba [manual]
El panel SHALL inicializar el ST7789 por SPI a 80 MHz con DMA y presentar un framebuffer lógico
de 240×320 px (ancho × alto) cuyo origen (0,0) es la esquina superior izquierda con el conector
USB arriba. Los colores SHALL verse correctos (fondo `0xF4F3F0` blanco roto, no negativo) y sin
desplazamientos ni bandas.

#### Scenario: Patrón de orientación
- **WHEN** el firmware de bring-up dibuja un rectángulo rojo en (0,0)-(40,40), uno verde en la
  esquina inferior derecha y el texto "USB ↑" centrado
- **THEN** con el panel sostenido con el USB arriba, el rojo está arriba a la izquierda, el verde
  abajo a la derecha, el texto se lee derecho y los colores coinciden con los nombrados

#### Scenario: Refresco sin artefactos
- **WHEN** se anima un objeto que cruza toda la pantalla durante 30 s
- **THEN** no aparecen rasgados (tearing) visibles, bandas ni píxeles basura, y la tasa de
  refresco medida por LVGL es ≥ 25 fps

### Requirement: Táctil alineado con la pantalla [manual]
El táctil FT5x06 (I2C 0x38, SDA 15, SCL 16, INT 47) SHALL entregar coordenadas en el mismo
sistema que la pantalla: tocar un punto dibujado devuelve ese punto con error ≤ 6 px.

#### Scenario: Cinco dianas
- **WHEN** el firmware de bring-up pinta dianas en las cuatro esquinas (a 20 px del borde) y en el
  centro y se tocan una a una
- **THEN** cada toque se registra dentro del círculo de 12 px de la diana y el log serie imprime
  la coordenada recibida

#### Scenario: Pulsación mantenida
- **WHEN** se mantiene el dedo sobre la pantalla 3 s
- **THEN** LVGL reporta estado PRESSED de forma continua (sin alternar a RELEASED) y al levantar
  reporta RELEASED en menos de 100 ms

### Requirement: Retroiluminación regulable [manual]
La retroiluminación (GPIO 38) SHALL controlarse por PWM (LEDC, 5 kHz, 8 bits) con un nivel
0–100 %. Al arrancar SHALL quedar al nivel persistido en ajustes (100 % por defecto).

#### Scenario: Rampa de brillo
- **WHEN** el firmware de bring-up recorre 10 %, 50 % y 100 % durante 1 s cada uno
- **THEN** el brillo cambia de forma visible y monótona, sin parpadeo perceptible

### Requirement: Zumbador con tono y volumen [manual]
El zumbador (GPIO 8) SHALL emitir tonos de frecuencia variable (200–4000 Hz) con ciclo de trabajo
configurable (LEDC 10 bits) en un temporizador LEDC distinto del de la retroiluminación, de modo
que cambiar el tono no altere el brillo.

#### Scenario: Escala y silencio
- **WHEN** se reproducen 523, 659, 784 y 1047 Hz durante 150 ms cada uno y luego silencio
- **THEN** se oyen cuatro notas ascendentes, el brillo de la pantalla no varía y tras la última
  nota no queda ningún zumbido residual

### Requirement: Botón BOOT legible en ejecución [manual]
El botón BOOT (GPIO 0, activo a nivel bajo, pull-up interno) SHALL poder leerse como entrada una
vez arrancado el firmware, sin interferir con el modo de descarga al reiniciar.

#### Scenario: Lectura del botón
- **WHEN** se pulsa BOOT con el firmware en marcha
- **THEN** el log serie imprime "BOOT pressed" al pulsar y "BOOT released" al soltar

### Requirement: PSRAM y memoria disponibles [test-unity]
El sistema SHALL detectar 8 MB de PSRAM octal al arrancar y dejar ≥ 7 MB libres en PSRAM y
≥ 150 KB libres en RAM interna tras inicializar pantalla, táctil y LVGL.

#### Scenario: Informe de memoria al arrancar
- **WHEN** termina la inicialización del hardware
- **THEN** el log muestra `psram: 8 MB` y los valores de heap libre interno y PSRAM cumplen los
  umbrales anteriores

### Requirement: Consola serie de diagnóstico [manual]
El firmware SHALL emitir el log de ESP-IDF por el puerto serie USB a 115200 baudios, con nivel
INFO por defecto, y SHALL imprimir al arrancar una línea `IncuTwin <versión> SN <serie>` seguida
del slot de arranque (`ota_0`/`ota_1`) y su estado de verificación.

#### Scenario: Línea de arranque
- **WHEN** el panel arranca con el monitor serie abierto
- **THEN** en los primeros 3 s aparece la línea de identificación con versión, serie y slot
