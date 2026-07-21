---
name: loop-modes
description: Las modalidades de ejecución del loop de Genesis (auto / human / oneshot) para motherBoard/Display_HMI/shared y sus puntos de aprobación humana. Usar al arrancar trabajo en una sesión, al decidir si parar a pedir aprobación, o ante dudas sobre cuánta autonomía aplicar. El modo activo vive en .claude/.loop-mode.
---

# Modalidades del loop

Genesis ejecuta el trabajo en una de **tres modalidades**. El modo activo se guarda en
`.claude/.loop-mode` (gitignored, personal, propio de esta capa Firmware/). Lo fija el usuario con
`/loop-mode <modo>`; si no está fijado, el hook `SessionStart` te pide preguntarlo antes de la primera
tarea. El **paso de learnings** (blindado por hook `require-retro.sh`) aplica en TODAS las modalidades.

**Gate de merge/release siempre activo, independiente del modo:** `guard-merge.sh` bloquea `git
merge`/`git tag v*` en **las tres modalidades** — firmware de dispositivo médico, la integración
irreversible a `dev`/`main` siempre exige la marca `.claude/.merge-approved` de un solo uso, incluso
en `auto`. No lo desactives ni lo condiciones a `.loop-mode` de nuevo.

## `auto` — full desatendido

Sin gate de plan/tarea previo ni de aprobación intermedia: exploras, ejecutas, verificas (`pio run
-e main` por placa afectada + `pio test -e native` en motherBoard donde aplique — el flasheo/prueba
en hardware real sigue siendo manual). Al llegar al merge, `guard-merge.sh` te para igual que en
`human` — muestra al usuario el estado (tareas hechas, build verde, diff resumido), espera su
aprobación explícita, crea la marca (`printf ok > .claude/.merge-approved`) y reintenta. Tras el
merge, sacas learnings y sigues. El humano interrumpe con `Esc`.

> A diferencia de `SensorBoard_v2/.claude/`, esta capa no incluye (todavía) un hook de
> re-despertar automático tipo `unattended-loop.sh` — si hace falta, se propone y añade como mejora
> concreta (skill `meta-self-improvement`), no se asume presente.

## `human` — human-in-the-loop (supervisado)

Metes al humano en el loop en **tres gates** (para y espera su visto bueno; no continúes sin él):

1. **Plan/feature aterrizada** — tras explorar, presenta el plan (o la spec) y **espera
   aprobación/feedback** antes de ejecutar nada. (Solo en `human`; en `auto` este paso no para.)
2. **Antes del merge** — cuando todas las tareas están hechas y el build está verde, **para antes
   del `merge --no-ff` a `dev`** y pide validación. `guard-merge.sh` lo bloquea si intentas mergear
   sin aprobación.
3. **Antes de cada release/tag** — antes de cortar `release/* → main` y `git tag`, pide aprobación.

Entre gates ejecutas con autonomía (loop TDD donde aplique, commits atómicos). El paso de learnings
se hace **tras** la validación del merge.

## `oneshot` — una tarea puntual

Para cambios triviales/menores sin loop: haces la tarea, verificas, commiteas (o mergeas si
procede), y **paras**. Apto para typos, bumps menores, fixes de 1 fichero. No exige spec. El gate de
learnings solo aplica si el cambio tocó la capa agéntica (`Firmware/.claude/`).

## Cómo se elige el proceso (multi-velocidad)

Ortogonal a la modalidad, escala el proceso al **tamaño del cambio** (ver `loop-engineering`):
trivial/menor → sin spec, loop abreviado u `oneshot`; medio/grande → loop completo, spec/ADR. Las
specs **no siempre son necesarias**.

## Cambiar de modo

`/loop-mode auto` · `/loop-mode human` · `/loop-mode oneshot` · `/loop-mode` (muestra el actual).
