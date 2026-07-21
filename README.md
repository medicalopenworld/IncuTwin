# IncuTwin FW — CrowPanel Advance 2.8"

Gemelo digital de la incubadora **IncuNest** sobre el
[Elecrow CrowPanel Advance 2.8"](https://github.com/Elecrow-RD/CrowPanel-Advance-2.8-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-320x240)
(ESP32-S3-WROOM-1-N16R8, pantalla IPS ST7789 320×240, táctil GT911),
montado **en vertical** (240×320, USB arriba).

Un bebé animado (respira, bosteza, duerme con Zzz, late su corazón) refleja
el estado real de la incubadora recibido de ThingsBoard a través de
Firebase. La UI está pensada para niños y personas mayores: sin números,
solo estados con colores e iconos grandes, en español e inglés.

## Características

- **Bebé animado** a partir de `Images/Baby.png`: respiración continua,
  bostezos aleatorios, dormido/despierto, corazón que late a la frecuencia
  real (sin mostrar el número).
- **Tono de piel configurable** (6 tonos) vía atributo de ThingsBoard
  (campo `skin` en Firebase) o localmente desde Ajustes. Implementado como
  intercambio de paleta en PSRAM: cambio instantáneo.
- **Efectos de iluminación en pantalla**: halo que "respira" detrás del
  bebé y cambia según el estado — azul calma, ámbar calentando, azul
  fototerapia, rojo pulsante en alarma.
- **Notificaciones retro por buzzer**: jingle 8-bit al arrancar, fanfarria
  cuando la incubadora detecta al bebé (`baby`) con 10 s de latido
  audible, y "lub-dub" al bpm real mientras tocas al bebé. Silenciable
  desde Ajustes (persistente).
- **Estados de uso** en 3 iconos circulares (Calor, Luz y Corazón): el
  color del borde indica el estado, sin texto ni números.
- **Barra superior**: icono WiFi con nivel de cobertura (4 niveles por
  RSSI; gris tachado sin WiFi, ámbar si hay WiFi pero la nube no
  responde) y engranaje de Ajustes.
- **Vista sin conexión**: cuando la IncuNest está apagada, sin vincular
  o sin bebé (`baby: false`), la pantalla muestra la imagen
  `IncuNest_empty` en lugar del bebé.
- **Barra de estado** inferior siempre visible: "Sin conexión WiFi",
  "Conectando a la nube...", "Sin IncuNest vinculada", "IncuNest
  apagada", "IncuNest sin bebé", "Bebé durmiendo/despierto" o
  "¡Alarma!", con su color.
- **Botón "Agarra mi mano"**: equivale a tocar al bebé (despierta al
  bebé y cuenta como interacción en la telemetría de uso).
- **ES/EN** conmutable en Ajustes (persistente, español por defecto).
- Colores y tipografía de marca tomados del logo `IncuTwin_logo.png`
  (wordmark renderizado como imagen).
- **Onboarding en el primer arranque**: idioma → QR para conectar el
  móvil (SoftAP + portal cautivo: WiFi, nombre, email, RGPD) → provisión
  automática en ThingsBoard → QR de vinculación con la app. Sin teclear
  nada en el panel. Ver [docs/PROVISIONING.md](docs/PROVISIONING.md).
- **ThingsBoard**: provisión automática (Device Provisioning), telemetría
  de uso (horas de encendido, de conexión y de "Agarra mi mano" — tocar
  al bebé) y **OTA** gestionada desde ThingsBoard.
- **Serialización en fabricación** con `tools/factory_provision.py`
  (NVS de fábrica + etiqueta QR imprimible + manifiesto CSV).
- Factory reset: mantener pulsado el engranaje y confirmar.

## Compilar y flashear

1. Instala [PlatformIO](https://platformio.org/) (VS Code o CLI).
2. Edita `include/config.h`: host de ThingsBoard (`TB_HOST`) y host/token
   de Firebase. El WiFi y los datos del usuario NO van aquí: los recoge
   el onboarding.
3. Conecta el panel por USB y ejecuta:

```bash
pio run -t upload -e crowpanel_advance_28
pio device monitor
```

> Si el puerto no aparece, mantén pulsado **BOOT** y pulsa **RESET** para
> entrar en modo descarga.

## Estructura

```
├── platformio.ini            # ESP32-S3 N16R8 + LVGL 8.3 + LovyanGFX
├── include/
│   ├── config.h              # WiFi / Firebase / ajustes de usuario
│   ├── pins_config.h         # pines del CrowPanel Advance 2.8
│   └── lv_conf.h             # configuración LVGL
├── src/
│   ├── main.cpp              # init pantalla/táctil/LVGL + servicios
│   ├── display/              # driver LovyanGFX (ST7789)
│   ├── net/                  # WiFi + cliente SSE de Firebase RTDB
│   ├── app/                  # modelo de estado compartido
│   ├── ui/                   # pantallas, tema, i18n, bebé animado
│   └── assets/               # imágenes generadas (no editar a mano)
├── tools/
│   ├── gen_assets.py         # Baby.png -> frames + 6 paletas de piel
│   ├── gen_extra.py          # wordmark + iconos de estado
│   └── factory_provision.py  # serialización en fabricación + etiqueta QR
└── docs/
    ├── FIREBASE_SETUP.md     # crear y configurar el proyecto Firebase
    ├── FIREBASE_SCHEMA.md    # esquema de datos y puente ThingsBoard
    └── PROVISIONING.md       # fabricación, onboarding y vinculación app
```

## Datos

El panel se suscribe por SSE a
`/incutwin/{DEVICE_ID}/state` en Firebase RTDB. El esquema del nodo y cómo
alimentarlo desde ThingsBoard están en
[docs/FIREBASE_SCHEMA.md](docs/FIREBASE_SCHEMA.md), incluido un `curl` para
probar sin incubadora.

## Regenerar los assets

Solo si cambias `Images/Baby.png` o el logo (requiere Python + Pillow):

```bash
python3 tools/gen_assets.py   # frames del bebé + paletas de piel
python3 tools/gen_extra.py    # wordmark + iconos + WiFi + IncuNest_empty
```

`gen_extra.py` también regenera la imagen de la incubadora vacía
(`Images/IncuNest_empty.png` → 240×320, derecha, con bandas blancas) y
los iconos de cobertura WiFi. Si el panel se monta girado al revés,
cambia `offset_rotation` (2 ↔ 0) en `src/display/LGFX_CrowPanel28.h`.

## Calibración táctil

Si el toque no coincide con la pantalla, ajusta `TOUCH_MAP_*` en
`include/pins_config.h` y activa `#define TOUCH_DEBUG` en `src/main.cpp`
para ver las coordenadas por serie.
