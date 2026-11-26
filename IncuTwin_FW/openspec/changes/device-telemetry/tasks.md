# device-telemetry — Plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** que cada IncuTwin registre sus eventos de uso en una cola persistente y los mande en lotes
a `incutwin/<id>/ev`, y que su foto `metrics` lleve lo que falta para el análisis.

**Architecture:** componente nuevo `components/telemetry`: `tel_logic.c` (puro: codificar eventos,
armar lotes, eventos de recompensas a partir de antes/después), `tel_queue.c` (cola en anillo sobre
una interfaz de flash, probada con un doble en RAM) y `telemetry.c` (pegamento: NVS, partición,
mutex, temporizador, PUBACK). Los demás componentes solo llaman a `telemetry_log()`.

**Tech Stack:** ESP-IDF 6, C, Unity en `test_apps` (on-target, COM12), esp-mqtt, `esp_partition`.

**Spec:** `proposal.md` y `specs/` de este cambio; diseño completo en
`incutwin-app/docs/superpowers/specs/2026-11-26-telemetria-uso-design.md` §5.2–5.4.

**Antes de empezar:** rama `feat/device-telemetry` desde `feat/acompanar` (0edb3ae ya trae
`hmi-corazones` e IncuPray). Incluye también el rol `collector` en `tools/broker_bootstrap.py`
(tarea 7 del plan del recolector, aparcada allí): ver `incutwin-app/docs/analitica.md`.

## Global Constraints

- La telemetría nunca bloquea la UI, nunca enseña errores ni alarmas; si algo falla, se registra en
  el log serie y se sigue.
- La demo no genera eventos salvo `demo` (`on`/`off`).
- Ningún dato personal en eventos: ni nombre del bebé, ni nombre de la IncuTwin, ni SSID.
- Lote: ≤ 20 eventos y ≤ 4096 bytes; un solo lote en vuelo; QoS 1, no retenido.
- Envío: al conectar, 60 s tras el primer evento sin enviar, o con 20 pendientes.
- Cola: ≥ 5.000 eventos en la partición `storage`; descarta el más viejo y suma `lost`.
- `seq` monótono de por vida; `gen` +1 por restablecimiento de fábrica; `b` +1 por arranque.
- `d` de cada evento ≤ 160 bytes de JSON.
- Commits en español, imperativo, pequeños.

## Review Focus

1. **Corte de luz mientras se escribe un evento en flash**: al arrancar, el registro a medias se
   ignora y la cola sigue sana. → test en Tarea 2.
2. **Reinicio con eventos ya enviados y confirmados**: no se reenvían (si no, el recolector los
   descarta, pero gastan datos). → test en Tarea 2.
3. **Muchos eventos en ráfaga** (cambiar de pestaña rápido): `screen` no repite la misma página en
   < 5 s y la UI no se nota. → test en Tarea 1 (filtro puro).
4. **Hora que aparece a mitad de arranque (SNTP llega tarde)**: los eventos de antes van con `t=0` y
   su `up`; los de después con hora. → test en Tarea 1.
5. **Restablecimiento de fábrica sin conexión**: el evento `reset` queda en la cola (que no se
   borra) y sale con el nuevo dueño, con el `gen` antiguo. → test en Tarea 2.

---

## 1. Lógica pura (`tel_logic`)

**Files:** Create `components/telemetry/include/tel_logic.h`, `components/telemetry/tel_logic.c`,
`components/telemetry/CMakeLists.txt`; Test `test_apps/main/test_telemetry.c` (+ `SRCS` e
`INCLUDE_DIRS` en `test_apps/main/CMakeLists.txt`).

**Interfaces (Produces):**

```c
#define TEL_KIND_MAX  12
#define TEL_D_MAX     160
#define TEL_BATCH_MAX 4096
#define TEL_BATCH_N   20
typedef struct {
    uint32_t seq;
    int64_t  t;                 /* epoch; 0 sin hora */
    uint32_t up;                /* s desde el arranque */
    char     kind[TEL_KIND_MAX];
    char     d[TEL_D_MAX];      /* objeto JSON ya armado, "{}" si nada */
} tel_event_t;

int tel_event_encode(char *out, size_t len, const tel_event_t *e);              /* -1 si no cabe */
/* Mete en out tantos eventos de ev[0..n) como quepan (≤ TEL_BATCH_N, ≤ len); *used = cuántos. */
int tel_batch_build(char *out, size_t len, uint32_t gen, uint32_t boot,
                    const tel_event_t *ev, int n, int *used);
/* Eventos de recompensas entre dos estados (+ las novedades de la operación). Llama a sink(kind, d)
 * en este orden: heart, seal (uno por bit), level, streak, passport, spend. */
typedef void (*tel_sink_t)(const char *kind, const char *d, void *ctx);
void tel_rewards_diff(const rw_state_t *before, const rw_state_t *after, const rw_news_t *news,
                      const char *why, tel_sink_t sink, void *ctx);
/* Filtro de pantallas: true si hay que registrar `page` (no la misma en < 5 s). */
bool tel_screen_should_log(int page, uint32_t now_ms, int *last_page, uint32_t *last_ms);
```

`tel_rewards_diff` decide:
- `heart` si `after.earned > before.earned`: `{"n":<diferencia>,"why":"<why>","bal":<after.balance>}`
- `seal` por cada bit nuevo de `seals`: `{"id":<bit>}`
- `level` si sube `rw_level(earned)`: `{"level":<n>}`
- `streak`: `up` si sube; `broken` si baja a ≤ 1 desde > 1; `saved` si baja `saved`;
  `{"ev":"…","n":<after.streak>}`
- `passport` por país nuevo o estrella nueva: `{"cc":"TG","star":0|1}`
- `spend` si `balance` baja sin `earned` que suba y cambia `unlocked`: `{"item":<bit>,"price":<n>,"bal":<n>}`

- [x] 1.1 Tests Unity que fallan:
  - `tel: evento codificado` → `{"s":7,"t":0,"up":100,"k":"boot","d":{"reason":"poweron","fw":"2.1.0"}}`
  - `tel: lote de 2` igual al escenario «Dos eventos» de la spec
  - `tel: lote parte a 20` (25 eventos → `used` 20) y `tel: lote parte por tamaño` (eventos con
    `d` de 160 B → `used` < 20 y `n` ≤ 4096)
  - `tel: diff de gesto con sello y nivel` → secuencia `heart`, `seal`, `level`
  - `tel: diff racha rota y abrazo guardado`, `tel: diff compra`, `tel: diff país nuevo con estrella`
  - `tel: pantalla repetida en < 5 s no se registra; otra pantalla sí`
  - `tel: t=0 antes de hora y t>0 después` (encode respeta `t`)
- [x] 1.2 `idf.py -C test_apps build flash monitor -p COM12` → los nuevos FAIL. (COM12, 26-nov, 14 `tel:` en FAIL, 92d51de)
- [x] 1.3 Implementa `tel_logic.c`.
- [x] 1.4 → todos los tests en verde (también los 146 de antes). (COM12, 26-nov, 172 tests, 0 fallos, 92d51de)
- [x] 1.5 Commit `Añade la lógica de eventos de telemetría con sus tests`.

## 2. Cola persistente (`tel_queue`)

**Files:** Create `components/telemetry/include/tel_queue.h`, `components/telemetry/tel_queue.c`;
Test en `test_apps/main/test_telemetry.c` con un doble de flash en RAM (`tel_flash_ram_*` en el
propio test).

**Interfaces (Produces):**

```c
typedef struct {   /* lo que la cola necesita de la flash; esp_partition en telemetry.c, RAM en test */
    bool (*read)(void *ctx, uint32_t off, void *buf, size_t len);
    bool (*write)(void *ctx, uint32_t off, const void *buf, size_t len);
    bool (*erase)(void *ctx, uint32_t off, size_t len);   /* sectores de 4096 */
    uint32_t size;                                        /* bytes usables */
    void *ctx;
} tel_flash_t;

typedef struct { /* privado; tamaño fijo para poder declararlo en estático */ ... } tel_queue_t;

/* Reconstruye la cola leyendo la flash; acked_seq = último seq confirmado (NVS). */
void     tel_queue_open(tel_queue_t *q, const tel_flash_t *f, uint32_t acked_seq);
uint32_t tel_queue_next_seq(const tel_queue_t *q);              /* max(seq visto, acked) + 1 */
bool     tel_queue_push(tel_queue_t *q, const tel_event_t *e);  /* e->seq ya asignado */
int      tel_queue_peek(const tel_queue_t *q, tel_event_t *out, int max); /* los más viejos sin confirmar */
void     tel_queue_ack_through(tel_queue_t *q, uint32_t seq);
uint32_t tel_queue_pending(const tel_queue_t *q);
uint32_t tel_queue_lost(const tel_queue_t *q);                  /* desde que se abrió */
```

Formato fijado por el plan: registros de 256 B (cabecera con magia, `seq`, longitud y CRC32 +
`tel_event_t`), 16 por sector, anillo sobre 1,25 MB de la partición `storage` (≥ 5.000 registros).
Un registro con CRC malo o a medias se ignora. Al llenarse, se borra el sector más viejo (sus
registros sin confirmar suman a `lost`).

- [x] 2.1 Tests Unity que fallan (doble de flash en RAM):
  - `telq: push, peek y ack`; `telq: reabrir conserva pendientes y next_seq` (300 eventos, reabrir 2
    veces, peek devuelve 1..300 en orden)
  - `telq: reabrir con acked_seq no devuelve los confirmados`
  - `telq: llena descarta los más viejos` (5.010 → pendientes ≥ 5.000, `lost` = lo descartado,
    el más viejo pendiente es el 11 o posterior y los últimos siempre están)
  - `telq: registro a medias se ignora` (escribir medio registro en el doble y reabrir)
  - `telq: next_seq tras borrar la NVS` (acked 0, flash con seq hasta 900 → 901)
- [x] 2.2 → FAIL (COM12, 26-nov, 181 tests, 9 `telq:` en FAIL). 2.3 Implementa `tel_queue.c`. 2.4 → verde (COM12, 26-nov, 181 y tras la ronda de corrección 184 tests, 0 fallos, 81b5ad1 y 5f22cd7).
- [x] 2.5 Commit `Añade la cola persistente de eventos de telemetría`.

## 3. PUBACK en `mqtt_link`

**Files:** Modify `components/mqtt_link/include/mqtt_link.h`, `components/mqtt_link/mqtt_link.c`,
`components/app_events/include/app_events.h`.

**Interfaces (Produces):**
- `int mqtt_link_publish_id(const char *topic, const char *payload, size_t len, int qos, bool retain);`
  → `msg_id` (> 0) o -1 sin sesión. `mqtt_link_publish()` queda igual.
- Evento `TWIN_EVT_MQTT_PUBACK` con `typedef struct { int msg_id; } app_evt_puback_t;` publicado en
  `MQTT_EVENT_PUBLISHED`.

- [x] 3.1 Implementa; `idf.py build` sin avisos nuevos.
- [x] 3.2 Commit `Avisa del PUBACK de los mensajes publicados`.

## 4. Pegamento `telemetry.c`, identidades y reset

**Files:** Create `components/telemetry/include/telemetry.h`, `components/telemetry/telemetry.c`;
Modify `components/storage/include/storage.h` (`STORAGE_NS_TEL "tel"`),
`components/storage/storage.c` (`storage_factory_reset` sube `tel/gen` y **no** borra `tel` ni la
partición), `main/app_main.c` (init tras `storage_init`, `start` tras `mqtt_link_start`),
`components/support_console/console_cmds.c` (comando `tel`), `partitions.csv` **no cambia**.

**Interfaces (Produces):**

```c
void     telemetry_init(void);                          /* abre NVS tel, partición, cola; b++ y evento boot/ota */
void     telemetry_start(void);                         /* se suscribe a MQTT y PUBACK; temporizador 60 s */
void     telemetry_log(const char *kind, const char *d_json); /* seguro entre tareas; nunca bloquea > 10 ms */
void     telemetry_set_demo(bool on);                   /* con on, telemetry_log ignora todo salvo "demo" */
uint32_t telemetry_gen(void), telemetry_boot(void), telemetry_pending(void), telemetry_lost(void);
uint32_t telemetry_demo_count(void);                    /* NVS tel/demo_n */
void     telemetry_flush_now(uint32_t wait_ms);         /* para el reset: intenta un lote y espera el PUBACK */
```

NVS `tel`: `gen` (empieza en 1), `boot_n`, `acked`, `fw` (último firmware visto: si cambia → evento
`ota {"from","to","ok":1}`; un arranque tras rollback → `ok:0`), `demo_n`.

- [x] 4.1 Implementa. Evento `boot {"reason":"<esp_reset_reason>","fw":"<versión>"}` en cada arranque;
  `net {"off_s":n}` al conectar tras ≥ 600 s sin broker.
- [x] 4.2 Comando serie `tel`: imprime `gen`, `boot_n`, `next_seq`, `pending`, `lost`, en vuelo.
- [x] 4.3 Flash en COM12 con el recolector local (o `mosquitto_sub -t 'incutwin/+/ev' -v` contra el
  broker de pruebas): tras arrancar llega un lote con `boot`; `tel` muestra `pending 0`. (COM12, 26-nov: lote de arranque con `boot`, PUBACK aceptado; da7f1b8 + 6d3a9be)
- [x] 4.4 Sin WiFi: generar eventos, reiniciar dos veces, conectar → llegan todos en orden, `seq`
  sin huecos. (COM12, 26-nov: sin WiFi y dos reinicios, `seq` 94..108 en orden, un lote por arranque; da7f1b8 + 6d3a9be)
- [x] 4.5 Commit `Registra y envía los eventos de telemetría con cola y PUBACK`.

## 5. Foto ampliada en `metrics`

**Files:** Modify `components/cloud_status/include/metrics_payload.h`,
`components/cloud_status/metrics_payload.c`, `components/cloud_status/cloud_status.c`,
`test_apps/main/test_metrics.c`, `components/cloud_status/CMakeLists.txt` (`telemetry`, `settings`).

**Interfaces:** `METRICS_PAYLOAD_MAX` pasa a 896. Nuevo tipo y firma:

```c
typedef struct {
    uint32_t gen, boot_n, demo_n, q, lost;
    const char *fw, *inc, *phase;      /* phase: "off|searching|waiting|with|ending|farewell" */
    uint8_t lang, vol, bright;
} metrics_extra_t;
int metrics_payload_build(char *out, size_t out_len, const metrics_usage_t *u, const rw_state_t *r,
                          const metrics_extra_t *x);
```

- [x] 5.1 Tests que fallan: los escenarios «Acompañando» y «Buscando» de la spec `device-metrics`
  de este cambio; «pasaporte lleno cabe» con `x` al máximo (`fw` de 31 caracteres, `inc` de 15).
- [x] 5.2 → FAIL (COM12, 26-nov). 5.3 Implementa; `cloud_status` rellena `x` con `accompany_current()`, la fase de
  `twin_model_get()`, `settings_*()` y `telemetry_*()`. 5.4 → verde (COM12, 26-nov, 187 tests, 0 fallos, 0f95ac2; antes de 43020ee).
- [x] 5.5 Commit `Amplía la foto de métricas con fase, IncuNest, ajustes y cola`.

## 6. Productores de eventos

**Files:** Modify `components/rewards/rewards.c`, `components/accompany/accompany.c`,
`components/ui/ui_nav.c`, `components/ui/ui_pages.c`, `components/ui/ui_onboarding.c`,
`components/settings/settings.c`, `components/demo_mode/*.c`, y sus `CMakeLists.txt`
(`PRIV_REQUIRES telemetry`). Consumes: `telemetry_log`, `tel_rewards_diff`, `tel_screen_should_log`.

| Dónde | Evento y `d` |
| --- | --- |
| `rewards.c`, tras cada operación real (no demo) | copia `before`, opera, `tel_rewards_diff(&before, &after, &news, why, …)` con `why` = `gesture`, `super`, `home`, `incupray`; `redeem {"item":"named_incunest"}` al canjear |
| `accompany.c`, en el gesto | `gesture {"sec":n,"inc":"<id>","super":0/1}` |
| `accompany.c`, con `ACC_R_PHASE` o `ACC_R_SWITCH` | `acc {"phase":"…","inc":"<id o "">","prev_inc":"…"}` |
| `ui_nav.c` `tab_clicked` y `ui_pages.c` al cambiar de panel | `screen {"p":"home|map|streak|hearts|notices|history|settings"}` filtrado con `tel_screen_should_log` |
| `ui_onboarding.c` `show_step` | `onboard {"step":"connect|wifi_try|server|done"}` |
| `settings.c` en cada `settings_set_*` con valor distinto | `setting {"key":"lang|vol|bright|quiet_on|quiet_from|quiet_to","val":n}` |
| `demo_mode` al entrar y salir | `telemetry_set_demo()`, `demo {"ev":"on"}` / `{"ev":"off","dur_s":n}`, `demo_n++` |
| `ui_nav.c` `reset_confirmed` y consola `factory_reset` | `reset {"gen":<gen actual>}` y `telemetry_flush_now(2000)` antes de `storage_factory_reset()` |

- [x] 6.1 Cablea los productores de la tabla; `idf.py build` sin avisos nuevos.
- [ ] 6.2 Flash en COM12 y prueba manual con `mosquitto_sub`: gesto → `gesture` + `heart` (+ `seal`
  la primera vez); cambiar de pestaña → `screen`; brillo → `setting`; demo → solo `demo on/off`;
  restablecer de fábrica → `reset` y después `gen` 2.
- [x] 6.3 IncuPray ya está en el firmware (9a10908, apagada): donde publica su participación,
  registra también `incupray {"id","ev":"join|leave|close","held_s","hearts"}` con `telemetry_log`
  (nunca en la IncuPray simulada de la demo).
- [x] 6.4 Commit `Registra gestos, recompensas, pantallas, ajustes, demo y reset`.

## 7. Documentación y cierre

- [x] 7.1 `docs/BROKER-MQTT-CONTEXT.md` §3: fila de `incutwin/<id>/ev`; `metrics` lo lee
  `collector`. (Hecho aquí también: rol `collector` en `tools/broker_bootstrap.py`, quitado `metrics` del gateway.)
- [ ] 7.2 `idf.py -C test_apps` en COM12: todo en verde; `idf.py build` sin avisos nuevos.
- [ ] 7.3 Prueba de extremo a extremo con el recolector local: un día de uso normal en COM12 →
  `events` sin huecos de `seq`, `snapshots` con `phase` e `inc`.
- [x] 7.4 `openspec validate device-telemetry` y commit `docs(telemetry): documenta el topic ev, el rol collector y las tareas pendientes en placa` (265fe6a).

## Pendiente en placa

COM12 está reservado por otra sesión; estos pasos se hacen juntos cuando quede libre. Los builds de
`idf.py build` y `idf.py -C test_apps build` están hechos y limpios (desde PowerShell).

- [ ] T5: Unity en placa tras `43020ee` (`idf.py -C test_apps` en COM12, todo en verde); publicación
  de `metrics` disparada por el temporizador; marca de agua de pila de la tarea de `esp_timer`
  (`uxTaskGetStackHighWaterMark`) con margen.
- [ ] 6.2: checklist manual de `.superpowers/sdd/tasks/task-6-report.md` («Pendiente 6.2» y ronda 1) con
  `mosquitto_sub -t 'incutwin/<id>/ev'`: gesto, pestañas, ajustes, demo, onboarding, `acc`, IncuPray,
  restablecimiento de fábrica.
- [ ] 7.2: `idf.py -C test_apps` en COM12, todo en verde.
- [ ] 7.3: un día de uso normal con el recolector local: `events` sin huecos de `seq`, `snapshots` con
  `phase` e `inc`.
- [ ] Desplegar en el broker real: `python tools/broker_bootstrap.py` (rol y cliente `collector`;
  `removeRoleACL gateway ... metrics` a mano; `persistence true` y `max_queued_messages` ~100000).
- [ ] Unity de la revisión final (solo compilados, falta correrlos en placa): `tel: diff compra del
  ultimo articulo y ultimo sello`, `telq: gen y boot mas altos de la cola tras borrar la NVS`,
  `telq: registro corto o con otro `d` no da gen ni boot`, `tel: tel_ids_floor solo sube`.
- [ ] Reconexión con un lote en vuelo (repetir tras el arreglo de la tarea 4, 6d3a9be): publicar un
  lote, cortar el WiFi antes del PUBACK y volver; el mismo lote (mismos `seq`) sale otra vez y el
  recolector descarta los duplicados; `tel` acaba en `pending 0` y «en vuelo: ninguno».
- [ ] Lote lleno de ~4 KB: 20 eventos con `d` largo (p. ej. 20 `setting` o `incupray` seguidos sin
  conexión). `mqtt_link` tiene `buffer.size` 2048, así que el envío sale fragmentado: comprobar que
  llega entero al broker, con un solo PUBACK, y que el recolector lo acepta.
- [ ] Marca de agua de pila: `tel` imprime «pila libre min» de la tarea `telemetry` (4608 B) y de
  `esp_timer` (3584 B). Mirarla tras un rato de uso con lotes y borrados de sector, y la de
  `esp_timer` justo después de una publicación de `metrics` disparada por el temporizador (la
  periódica o la de «recompensas»). Margen esperado ≥ 512 B en las dos.
- [ ] Tirones de la UI durante el borrado de un sector (30-400 ms, cada 16 eventos): con muchos
  eventos seguidos (pestañas, ajustes), la animación de cambio de pantalla y el latido no se cortan.
- [ ] Borrado de la NVS con la cola viva (arreglo de la revisión final): `nvs_flash_erase` de la
  partición `nvs` (no de `storage`) y reaprovisionar; al arrancar sale el aviso «NVS por detrás de
  la cola» y el `boot` lleva el `gen` de antes y un `b` mayor que todos los de la cola.

## Al fusionar con la tienda (`feat/acompanar`)

- [x] `demo_mode_set()`: las dos ramas la definen. Se queda la de esta rama (la que registra `demo`
  y llama a `telemetry_set_demo`); lo que la de la tienda hace además (reiniciar el escenario,
  pitido) va a `on_long` o al comando de consola. Si se dejan las dos, una llama a la otra y se
  repite sin fin. *Hecho (merge de feat/acompanar):* lo de la tienda vive en `demo_mode_button(on)`
  (demo_mode_set + primer escenario al entrar + pitido); la usan `on_long` y el comando `demo on|off`
  de la tienda. `sim_state demo-on/off`, `incupray demo|stop` y `demo_mode_factory_reset` siguen
  llamando a `demo_mode_set`, el único sitio que entra/sale y registra.
- [x] `spend` y `redeem`: volver a colgarlos del resultado de éxito `rw_unlock_t` de la tienda (en
  vez del diff de saldo y `unlocked` de aquí). `rewards_tel.c` ya toma la anchura de `unlocked` del
  campo, así que con `uint64_t` y ~26 artículos sigue viendo todos los bits. *Hecho:* en
  `rewards_unlock` solo con `RW_UNLOCK_OK` y fuera de la demo se llama a `tel_rewards_diff(…, "unlock")`
  y, si es la IncuNest con tu nombre, `redeem`. `spend` sigue saliendo solo del diff (una fuente, ya
  probada en test_telemetry): solo hay compra si baja el saldo y aparece el bit, así que elegir o
  poner/quitar algo ya suyo no emite, y no hay `spend` explícito que lo duplique. La apuesta de la
  IncuPray (`rewards_spend`) sigue sin evento.
- [x] `metrics_payload.c`: unir los campos de las dos ramas y volver a medir `METRICS_PAYLOAD_MAX`
  (≥ 896) con todo al máximo. *Hecho:* campos de la tienda (`hat_boy`, `hat_girl`, `blanket`,
  `santa`, `snow`, `unlocked` con `%llu`) tras `named_incunest` y luego los de telemetría. Medido
  749 B con todo al máximo (ts de 20 cifras, `unlocked` = UINT64_MAX, fw 47 y inc 23); se queda en 896.
- [ ] `idf.py -C test_apps` en placa con las dos ramas juntas.

