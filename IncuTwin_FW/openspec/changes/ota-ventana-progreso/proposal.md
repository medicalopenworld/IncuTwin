# ota-ventana-progreso — La OTA se ve: ventana con barra de progreso, en el momento oportuno

## Why

La OTA descargaba en silencio y reiniciaba 1 s después de verificar, aunque la familia estuviera
a mitad de «Coge mi mano», y el reinicio se veía como una pantalla negra que parece una avería.
Pablo (26-nov-2026) decide que la actualización se vea: una ventana con una barra de progreso que
reinicia en cuanto acaba, y que salga en un momento que no moleste.

## What Changes

- Al llegar un `cmd/ota` válido (o vencer el retardo de flota) el panel **no descarga todavía**:
  espera a abrir su ventana.
- **Cuándo sale la ventana**: en seguida si el panel acaba de arrancar y nadie ha tocado la
  pantalla (5 min); si no, tras 2 min sin tocar la pantalla y sin «Coge mi mano» a medias; si en
  6 h no se ha dado, en cuanto no haya gesto a medias.
- **La ventana**: modal sobre todo (fondo oscurecido), «Actualizando tu IncuTwin», «Tardará un
  minuto. No la desenchufes.», barra de progreso y porcentaje. Al terminar, «¡Lista!
  Reiniciando...» y el panel reinicia en ~1,5 s.
- **Si se corta** (luz, red): la OTA queda guardada y se reintenta sola al reconectar, hasta 3
  descargas.
- **Si falla** (red, hash): «No se ha podido actualizar. Tu IncuTwin sigue como estaba.» 4 s y
  todo sigue igual; un `cmd/ota` posterior vuelve a intentarlo.
- **Firmware objetivo retenido** (`cmd/fw`, como el «firmware objetivo» de ThingsBoard pero por
  el broker): cada panel que va por detrás se actualiza al conectar, también los que estaban
  apagados; nunca baja; el objetivo de un panel manda sobre el de la flota (pilotos); un binario
  que falló no se reintenta. `tools/release.py --set-target <SN|all>` y `--clear-target`.
- Build `dev`: además de aceptar cualquier host, no comprueba el certificado, para probar la OTA
  con un servidor HTTPS en el PC.

## Capabilities

### New Capabilities

### Modified Capabilities

- `ota-update`: cuándo empieza la descarga, ventana con progreso y reinicio inmediato al acabar.

## Impact

- Firmware: `ota_update` (cola hasta `ota_update_go()`, `ota_update_progress()`,
  `TWIN_EVT_OTA_PENDING`, reinicio al acabar; certificado opcional en la build `dev`),
  `app_events`, `ui` (`ui_ota.c`: cuándo, ventana y progreso), `sdkconfig.defaults.dev`.
- Sin cambios en el broker, `cmd/ota` ni `tools/release.py`.
