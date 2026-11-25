# Navegación y avisos en la pantalla de la IncuTwin — diseño

Fecha: 24-nov-2026 · Aprobado por Pablo el 24-nov-2026 · Base: rama `test/baby-sex-acompanar` (firmware ESP-IDF 6 +
LVGL 9, `components/`). Rama de trabajo: `feat/hmi-navegacion-avisos`.

## Para qué
Quien tiene una IncuTwin quiere moverse entre su IncuNest, lo que ha pasado y los ajustes con un toque, y enterarse
de lo que vive el bebé sin estar mirando la pantalla. Tono cálido, sin sustos: nunca se avisa de averías ni alarmas.

## Objetivos, fuera de alcance y lo que sería un fracaso
- **Objetivos:** barra fija de navegación con tres botones (Inicio, Historial 🐣, Ajustes); pantalla de Historial con
  los últimos eventos guardados en el panel; aviso en pantalla en cada cambio de lo que vive el bebé; buzzer solo en el
  ingreso y en «con sus papás»; horario nocturno configurable (21:00–9:00 por defecto).
- **Fuera:** varias IncuNest por IncuTwin (ver *Pendiente*), historial de estancias desde ThingsBoard
  (`tb-stay-history`), apagar o atenuar la pantalla de noche, avisos de conexión, alarma o pulsioxímetro.
- **Fracaso aunque funcione:** que suene de noche; un aviso falso al reiniciar o al volver la conexión; un aviso que
  asuste; que la barra tape algo de Inicio o que «Coge mi mano» quede inaccesible.

## Enfoques considerados
1. **Barra fija abajo en cada pantalla (elegido).** Tres botones siempre visibles; es lo que Pablo pidió y cabe en
   240×320 si Inicio se reordena.
2. Botones en la barra de arriba (junto al WiFi): no quita alto a Inicio, pero los botones serían pequeños (≈ 30 px)
   para el táctil y Pablo aprobó el esquema con la barra abajo.
3. Deslizar entre pantallas: ya se usa el deslizamiento para las páginas de la demo y los toques fantasma del táctil
   lo harían poco fiable.

## Navegación
```
┌──────────────────────┐
│ ᯤ   IncuTwin         │  barra superior sin la rueda dentada
│      [ bebé ♥ ]      │  Inicio: bebé, país, nombre, estado
│   🔥  ☀  [Coge mi    │  fila de iconos y «Coge mi mano»
│   (estado)    mano]  │
├──────────────────────┤
│  ⌂     🐣•     ⚙     │  barra fija ≈ 40 px; • = eventos sin ver
└──────────────────────┘
```
- Botones: casa (Inicio), pollito 🐣 (Historial, imagen propia generada con PIL: la pantalla no tiene fuente de
  emojis), rueda dentada (Ajustes). El activo, resaltado; los demás, atenuados. Cambio de pantalla con el mismo
  deslizamiento de 220 ms que hoy (izquierda o derecha según el orden de los botones).
- La pulsación larga que hoy abre el borrado de fábrica en la rueda dentada pasa al botón Ajustes de la barra.
- Ajustes pierde el botón «atrás» (la barra lo sustituye).
- La barra no aparece en el splash, el onboarding, la despedida ni en los diálogos.

## Historial
- El panel guarda en NVS los **50 últimos avisos** (anillo; sobreviven a un reinicio): tipo, hora (epoch de SNTP; 0 si
  aún no hay hora) e id de la IncuNest (no se muestra; deja la puerta abierta a varias IncuNest).
- La pantalla lista del más reciente al más antiguo: icono, texto del aviso y tiempo relativo («hace 2 h»; «—» sin
  hora). Vacía: pollito y «Aquí verás lo que vaya pasando en tu IncuNest».
- Punto en el pollito si hay avisos sin ver; se marcan vistos al abrir el Historial.
- Ni la demo ni el modo sim de prueba manual cuentan como avisos reales: la demo no escribe en el Historial.

## Avisos

| Cambio (estado de la IncuNest) | Aviso | Texto (es / en) | Buzzer |
|---|---|---|---|
| bebé `none`/`home` → `in` o `parents` | `ADMISSION` | «Ha entrado un bebé en la incubadora» / «A baby has entered the incubator» | melodía «bebé detectado» (`SOUND_BABY`) |
| bebé → `parents` desde `in` | `PARENTS` | «El bebé está con sus papás» / «The baby is with their parents» | fanfarria (`SOUND_PARENTS`) |
| bebé `in`/`parents` → `none` o `home` | `EXIT` | «El bebé ha salido de la incubadora» / «The baby has left the incubator» | — |
| calor (`thermo` ≠ off) empieza / acaba | `HEAT_ON` / `HEAT_OFF` | «Ha empezado el calor» / «Ha terminado el calor» · «Warmth has started» / «Warmth has ended» | — |
| fototerapia empieza / acaba | `PHOTO_ON` / `PHOTO_OFF` | «Ha empezado la fototerapia» / «Ha terminado la fototerapia» · «Phototherapy has started» / «Phototherapy has ended» | — |
| `parents` → `in`, sin conexión, `thermo = alarm` (si ya había calor), pulsioxímetro, nombre, sexo | — | — | — |

- **Aviso en pantalla:** franja que baja desde arriba (capa superior de LVGL) con icono y texto, 4 s; si llegan varios
  a la vez, uno detrás de otro. Tocarla abre el Historial. Aparece también de noche.
- **Horario nocturno:** uno para todo el panel, en Ajustes: activado/desactivado y horas «desde» y «hasta» (en horas
  enteras), **por defecto activado de 21 a 9**, hora de España (`CET-1CEST,M3.5.0,M10.5.0/3`). De noche el buzzer no
  toca melodías automáticas; el clic y el latido al tocar siguen sonando. El modo acompañar usa este mismo horario
  (antes 22–9 fijo).
- **Sin avisos falsos:** los avisos se calculan contra el último estado **en línea**; mensajes repetidos
  (`event_seq` ≤ último) no avisan; al arrancar, el primer mensaje solo puede dar avisos del bebé (el estado del bebé
  se guarda ya en NVS), nunca de calor o fototerapia; al cambiar de IncuNest emparejada (o en acompañar) se toma el
  nuevo estado como base sin avisar.

**Cambios de comportamiento respecto a hoy** (consecuencia directa de la tabla aprobada):
- Ir a casa (`home`) deja de tocar la fanfarria: es una salida, en silencio.
- Volver de `parents` a `in` deja de tocar «bebé detectado».

## Pendiente para versiones futuras
- **Una IncuTwin por persona con todas sus IncuNest** (una por cada suscripción de 20 €): emparejamiento con lista,
  deslizar entre IncuNest en Inicio, buzzer solo en ingreso y «con sus papás» con un tope de un sonido cada 10 min
  entre todas, y un interruptor por IncuNest en Ajustes. Decidido por Pablo el 24-nov-2026 y aplazado.
- Historial de estancias desde ThingsBoard (`tb-stay-history` / `hmi-stay-history`).

## Decisiones tomadas por Claude (corregibles)
- Horario nocturno en horas enteras (selector más sencillo en 240×320).
- 50 avisos en el anillo; 4 s por aviso en pantalla.
- La barra va en cada pantalla (no en la capa superior), para no taparla en splash, onboarding y diálogos.
- Pollito dibujado con PIL en `tools/gen_extra.py` (sin descargar imágenes de terceros).
- La demo enseña el aviso y suena, pero no escribe en el Historial.
