# hmi-accompany — Modo "Acompañar": la IncuTwin va con la IncuNest que tenga un bebé

## Why

Una IncuNest puede pasar días o semanas vacía entre un bebé y otro. Una IncuTwin emparejada a
una sola incubadora enseña entonces una incubadora vacía, y así no se crea el hábito que busca
la campaña de socios: **acompañar a un bebé todos los días**. Pablo (23-nov-2026) pide un modo en
el que la IncuTwin se conecte sola a una IncuNest que tenga un bebé en ese momento, guarde un
historial del tiempo acompañado y prefiera la IncuNest que más ha acompañado (continuidad con
"su" hospital y "su" país).

## What Changes

- **Nuevo modo de emparejado "Acompañar"**: `cmd/pair` retenido con `{"mode":"accompany"}`.
  Un `cmd/pair` con `incubator_id` o vacío lo apaga. El modo se guarda en NVS (`mqtt/acc_mode`).
- **La IncuTwin elige**: en este modo se suscribe a `incubators/+/state`, guarda el último estado
  de cada IncuNest (PSRAM) y elige, entre las que tienen bebé dentro o con sus papás, la que más
  tiempo ha acompañado (desempate pseudoaleatorio por panel, para repartir los paneles).
- **No abandona a un bebé a mitad de estancia**: la preferencia solo cuenta al elegir. Se cambia
  de IncuNest cuando el bebé se va a casa (tras la despedida) o cuando su IncuNest lleva 2 h sin
  bebé o desconectada.
- **Acompañar empieza con un gesto**: el tiempo solo cuenta los días en que se ha completado
  "Coge mi mano" con ese bebé, y mientras el bebé se ve en pantalla. Historial por IncuNest
  (segundos acompañados, máx. 32 IncuNest) en NVS `acc`.
- **Avisos en la IncuTwin**:
  1. *Se va a casa*: la vista de casa con fanfarria y "¡Se ha ido a casa! Gracias por
     acompañarle"; se queda hasta que se toca la pantalla o hasta el día siguiente.
  2. *Fin sin alta*: mensaje neutro "Ha terminado su estancia", sin sonido y sin celebrar.
  3. *Buscando*: "Ahora no hay bebés. Te avisaré".
  4. *Un bebé te espera*: melodía corta, halo que llama y texto hasta el primer gesto.
  5. *Recordatorio diario*: a las 19:00 (hora de España), si hoy no ha habido gesto, una llamada
     suave una sola vez y "¿Le das la mano hoy?" hasta el gesto.
  - Entre las 22:00 y las 9:00, en este modo, las melodías automáticas no suenan (solo aviso
    visual). Se respeta el volumen de Ajustes.
- **Siete corazones** en la página del país: los días de los últimos 7 en que has acompañado.
- `status` añade `"acc":1` y lleva en `incubator_id` la IncuNest que se acompaña ahora;
  "Coge mi mano" se publica con esa IncuNest.
- `tools/pair.py <panel> accompany` y `tools/unpair.py`: rol `incubators-all`
  (`subscribePattern incubators/+/state`).

## Capabilities

### New Capabilities

- `accompany`: elección de IncuNest, fases, historial, días acompañados, avisos y horario.

### Modified Capabilities

- `pairing`: `cmd/pair` acepta `{"mode":"accompany"}`.
- `twin-display`: textos de la barra de estado y halo del modo; corazones en la página del país.
- `sound`: horario de silencio en este modo.
- `device-status`: `acc` en `status`.
- `fleet-tools`: `pair.py accompany`.

## Impact

- Firmware: componente nuevo `components/accompany` (`acc_logic.c` puro y probado en
  `test_apps`, `accompany.c` pegamento), `commands`, `mqtt_link`, `twin_model`, `twin_types`,
  `ui`, `sound`, `cloud_status`, `storage` (blob y borrado en reset de fábrica), `main`.
- Broker: rol `incubators-all` para los paneles en este modo.
- **Privacidad (pendiente, fase 2)**: con este rol el panel recibe el estado de todas las
  IncuNest, nombre y peso incluidos si se comparten. Mejor que ThingsBoard publique una versión
  sin datos personales para este modo, o que la elección pase al servidor.
- **ThingsBoard (pendiente)**: la relación `MirroredBy`, el historial `itw_history` y el
  "Coge mi mano" resuelven la IncuNest por la relación de `pair.py`; en este modo deberían usar
  el `incubator_id` de `status` y de `hand`.
