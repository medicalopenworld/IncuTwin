# hmi-corazones — Diseño

## Context

- La racha ya existe en modo Acompañar (`accompany`): máscaras de 7 días (`acc_week`, `acc_super`,
  `acc_rest`) en el snapshot, súper corazón a los 3 min y días sin bebés que no la rompen.
- «Coge mi mano» completado llega por `TWIN_EVT_ACC_INPUT` (`APP_ACC_GESTURE`, `APP_ACC_SUPER`),
  que la UI solo publica fuera de la demo.
- `stay_log` guarda las estancias por IncuNest a partir del estado (bebé dentro, salida, stay_id).
- Pantalla principal con páginas deslizables (`ui_pages`): bebé, mapa y racha (o logros simulados
  en la demo). Ajustes está lleno hasta el pie (y 245..277) y la barra de navegación es común.
- Reglas de producto (Pablo, 25-nov-2026): se premia lo que hace quien acompaña, sin culpa ni
  pérdidas, sin competir ni pagar; la IncuTwin nunca enseña alarmas.

## Goals / Non-Goals

**Goals:**
- Saldo de corazones, racha larga con comodín, niveles, sellos, pasaporte y personalización,
  todo en el panel y sin servidor.
- Lógica pura con tests en `test_apps`; persistencia en NVS a prueba de reinicios.
- Que la demo de las ferias lo enseñe sin tocar lo de verdad.

**Non-Goals:**
- Guardar o sincronizar corazones en el servidor, verlos en la app móvil (fase posterior).
- IncuPray (cambio `incupray`).
- Rankings o cualquier comparación entre personas.

## Decisions

### 1. Componente `rewards` propio, no dentro de `accompany`
`components/rewards`: `rewards_logic.c` (puro: estado + funciones `on_gesture`, `on_super`,
`on_home`, `unlock`, `select`, `day_tick`) y `rewards.c` (NVS, mutex, bus).
- *Alternativa descartada*: meterlo en `accompany`. Acompañar elige IncuNest; los corazones
  también valen emparejada fija y en la IncuPray, y mezclarlo engorda un módulo ya grande.

### 2. Entradas: los mismos eventos que la racha, más el snapshot
- `TWIN_EVT_ACC_INPUT` GESTURE / SUPER → gesto completado y súper corazón (ya son solo reales).
- `TWIN_EVT_STATE_CHANGED` (fuera de la demo) → país de la IncuNest, estancia actual, paso a
  `home` y la máscara de días sin bebés (`acc_rest`) para no romper la racha.
- *Alternativa descartada*: contar en la UI. La UI no persiste y el bus ya desacopla.

### 3. El día es el día local de España, y sin hora no se cuenta
`localtime_r` con la TZ que pone `app_main` (`CET-1CEST`). Se guarda el último día con gesto
(días desde epoch local) para no sumar dos veces tras un reinicio. Sin SNTP no se suma nada.
- *Alternativa descartada*: contar en UTC. Partiría el día a la 1:00 / 2:00 en España.

### 4. La identidad del bebé es la estancia de `stay_log`
«Tres bebés» y «A casa» necesitan saber a qué bebé se dio la mano. Se usa la estancia abierta de
la IncuNest actual en `stay_log` (hash de IncuNest + stay_id, o + hora de llegada si no hay
stay_id). `rewards` guarda la última estancia con gesto y si ya contó; `home` sobre esa estancia
da el sello y la estrella del país una sola vez.
- *Alternativa descartada*: la clave de `baby_name`. Es local a la estancia actual y se borra en
  el reset; `stay_log` ya es la fuente de las estancias del Historial.

### 5. Persistencia: un blob versionado en NVS `rewards`
Struct con versión (saldo u16, ganado u32, racha u16, último día, abrazos u8, sellos bits,
pasaporte 32 × {iso[2], estrella}, días de diciembre bits, desbloqueos bits, elegidos). ~120 B.
Se guarda en cuanto cambia (unas pocas escrituras al día). Versión distinta → se descarta.
`STORAGE_NS_REWARDS_SIM` para el build sim. `storage_factory_reset` borra el namespace.
- *Alternativa descartada*: claves sueltas. Más escrituras y estados a medias tras un corte.

### 6. Demo: copia en RAM
Al entrar en la demo, `rewards` crea un estado sembrado (12 corazones, dos sellos, CM) y la UI lee
ese mientras `demo` está activo; los gestos de la demo (que hoy no publican GESTURE) los suma la UI
directamente a esa copia con la misma lógica pura. Al salir se tira.
- *Alternativa descartada*: el estado real con «deshacer». Riesgo de que se cuele algo.

### 7. Pantalla
- **Páginas**: `UI_PAGES_COUNT` pasa a 4: bebé, mapa, racha (o los logros simulados en la demo) y
  Logros (sellos + pasaporte). Logros siempre existe fuera de la demo, aunque no haya racha.
- **Racha**: el saldo, el nivel y los abrazos guardados en una fila encima de los 7 corazones.
- **Corazón que vuela** (`ui_streak_fx`): lleva «+1»; al súper corazón sale «+2» sobre la caja.
- **Franja**: la cola de `ui_notice` pasa a tener tipo (aviso del bebé / recompensa). Las
  recompensas no van a `event_log` y su toque lleva a Logros.
- **Ajustes**: pasa a lista con scroll (y 40..280) con «Personalizar ♥ 23 ›» arriba del todo;
  Personalizar es una subpantalla con «‹» (como el segundo nivel del Historial) y la barra de
  navegación sigue con Ajustes activo.
  - *Alternativa descartada*: un icono en el título de Ajustes. No se descubre.
- **Colores**: el halo en reposo y el fondo de Inicio salen de una tabla de temas; calor,
  fototerapia y «te llama» siguen en `ui_theme.h`.
- **Guirnalda**: un asset nuevo de `tools/gen_extra.py` (240 × 22, ARGB) detrás de la marca y del
  botón del país.

### 8. Tareas y eventos
Sin tareas nuevas: los handlers de `rewards` corren en la tarea del bucle de eventos de
`app_events` (la misma prioridad, core y pila que hoy); la escritura en NVS (< 10 ms) va ahí. La UI
nunca llama a NVS: lee una copia con `rewards_copy()` bajo mutex al recibir
`TWIN_EVT_REWARDS_CHANGED` (payload: bits de novedades — sello, nivel, guirnalda — para la franja).

```
UI (gesto) ─ACC_INPUT─▶ accompany (racha 7 días)
                      └▶ rewards ─NVS─▶ blob
twin_model ─STATE─────▶ rewards (país, estancia, home, días sin bebés)
rewards ─REWARDS_CHANGED{novedades}─▶ ui: racha, Logros, franja
```

## Risks / Trade-offs

- [Reset de fábrica = se pierde todo] → aceptado en esta fase (la IncuTwin puede cambiar de
  manos); el servidor lo guardará en una fase posterior.
- [Sin stay_id, una desconexión larga puede partir una estancia en dos y contar dos bebés] →
  `stay_log` no cierra estancias con la IncuNest desconectada; el riesgo queda en un corte de
  estado raro.
- [Precios mal calibrados] → constantes en un solo sitio; revisar tras dos semanas de uso real.
- [Ajustes con scroll es menos directo] → Personalizar arriba; el resto mantiene su orden.
- [La franja se llena con varias novedades a la vez] → la cola ya va de una en una (máx. 8).

## Migration Plan

Sin datos previos que migrar: el blob nace vacío (0 corazones). La racha de 7 días de `accompany`
siembra la racha larga al arrancar por primera vez (días seguidos de su máscara), para que nadie
empiece de cero si ya llevaba racha. Rollback: un firmware anterior ignora el namespace `rewards`.

## Open Questions

- ¿Sembrar también corazones por la racha que ya se llevaba? (Propuesta: no; solo la racha.)
- ¿Cuándo pasan los corazones al servidor y a la app? (Con o después de `incupray`.)
