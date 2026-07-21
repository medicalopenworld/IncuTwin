---
description: Orquesta el ciclo completo de loop engineering para una feature de motherBoard/Display_HMI/shared (explore→propose→design→red→green→refactor→verify→review→docs→archive→retro→merge). Usar cuando el usuario pida desarrollar una feature de principio a fin.
argument-hint: "<descripción de la feature>"
disable-model-invocation: true
allowed-tools: Bash(git *), Bash(pio *), Read, Write, Edit, Grep, Glob, Task
---

# /loop-run — ciclo de desarrollo orquestado

Feature solicitada: **$ARGUMENTS**

Estado git actual:

- Rama: !`git branch --show-current`
- Cambios sin commitear: !`git status --short | wc -l | tr -d ' '`

## Ejecución

Sigue el skill `loop-engineering` al pie de la letra. Antes de empezar, identifica qué placa(s) toca
la feature (`motherBoard`, `Display_HMI`, o `shared/` con impacto en ambas), asegúrate de partir de
`dev` limpio y crea la rama de feature (o invoca `/git-feature-start <slug>`).

Ejecuta los stages en orden, **un commit atómico por stage** (tipos en el skill `git-flow`). Delega cada stage al rol adecuado vía la tool Task (subagente):

1. **Explore** → `architect` + `researcher` (`openspec-explore`). Commit `chore(spec): explore`.
2. **Propose** → `product-manager` (`openspec-propose "$ARGUMENTS"`, nombrando el change `mb-`/`hmi-`/`shared-<slug>`). Commit `docs(spec): propose`.
3. **Design** → `architect` (+ `scribe` si hay ADR). Commit `docs(adr|spec)`.
4. **Red** → `qa-engineer` + `test-writer` (solo donde hay entorno `native`; si no, documenta verificación manual). Commit `test: failing specs`.
5. **Green** → `senior-developer`. Mínimo a verde. Commit `feat:`.
6. **Refactor** → `senior-developer`. Commit `refactor:`.
7. **Verify** → `qa-engineer` ejecuta `pio run -e main` (por placa afectada) + `pio test -e native` en `motherBoard` si aplica + comprueba escenarios testeables. La prueba en hardware/UI real es manual, fuera del loop automatizado. Commit `test: verify`.
8. **Review** → `code-reviewer` + `security-reviewer` EN PARALELO (un solo mensaje con dos Task). Resuelve hallazgos. Commit `fix: review feedback`.
9. **Docs** → `doc-keeper` (+ `scribe`). Commit `docs:`.
10. **Archive** → `product-manager` (`openspec-archive-change`). Commit `chore(spec): archive`.
11. **Retro + automejora** → `retro-improver`. Commit `chore(meta): retro + improve .claude`.
12. **Finish** → invoca `/git-feature-finish` (merge --no-ff a `dev`).

## Gates (no saltar)

- No pases de Propose sin escenarios testeables (los no testeables se marcan explícitamente como verificación manual).
- No empieces Green sin tests en rojo donde hay entorno `native` (TDD).
- No pases a Review sin `pio run -e main` (y `pio test -e native` si aplica) en verde.
- Tras cada stage, confirma que el commit se creó con el tipo Conventional correcto.

Al terminar, reporta: rama, lista de commits por stage, resultado del build/test y resumen de la automejora aplicada.
