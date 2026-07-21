---
name: spec-driven-development
description: Cómo hacer desarrollo guiado por specs (SDD) con OpenSpec en Genesis para motherBoard/Display_HMI/shared, y ligarlo a TDD. Usar al crear una propuesta de cambio, escribir o leer specs, entender las skills openspec-*, o conectar los escenarios de la spec con los tests.
---

# Spec-Driven Development con OpenSpec (capa Firmware/motherBoard+Display_HMI+shared)

OpenSpec mantiene una **spec viva** (`Firmware/openspec/specs/`) como fuente de verdad del
comportamiento actual, y gestiona cada modificación como un **change** con deltas auditables. Esta
instancia es **independiente** de `SensorBoard_v2/openspec/` — no comparten estado.

## Estructura

```
Firmware/openspec/
├─ specs/<dominio>/spec.md        # comportamiento ACTUAL (fuente de verdad — protegido)
└─ changes/
   ├─ <mb|hmi|shared>-<slug>/
   │  ├─ proposal.md              # por qué, alcance
   │  ├─ design.md                # enfoque técnico
   │  ├─ tasks.md                 # checklist con checkboxes
   │  └─ specs/<dominio>/spec.md  # DELTA: ## ADDED / ## MODIFIED / ## REMOVED Requirements
   └─ archive/<date>-<change>/    # changes completados (deltas ya fusionados)
```

## Skills de OpenSpec

OpenSpec está en **delivery mode `skills`**: cada paso es una skill `openspec-*` auto-invocable.

| Skill                     | Stage | Función                                                  |
| ------------------------- | ----- | ---------------------------------------------------------- |
| `openspec-explore`        | 1     | Socio de pensamiento: lee el código, sopesa opciones       |
| `openspec-propose`        | 2     | Crea el change (proposal + design + tasks + delta specs)   |
| `openspec-apply-change`   | 5     | Implementa según las tasks                                  |
| `openspec-archive-change` | 10    | Fusiona deltas en `specs/` y archiva el change              |
| `openspec-sync-specs`     | —     | Sincroniza deltas a `specs/` sin archivar (opcional)        |

> **Verify (stage 7) no es un paso de OpenSpec**: lo hace `qa-engineer` ejecutando `pio run -e main`
> (por placa afectada) + `pio test -e native` en `motherBoard` cuando aplica, y comprobando que cada
> escenario testeable de la spec tiene un test (ver skill `tdd-cycle`).

## Comandos reales del CLI

```bash
openspec new change <mb|hmi|shared>-<slug>   # crea el change (proposal/design/tasks/specs scaffold)
openspec instructions <artifact> --change <slug> --json   # instrucciones por artefacto
openspec status --change <slug> --json       # progreso, artefactos pendientes
openspec list --json                          # changes activos
openspec archive <change-name>                # fusiona los deltas en openspec/specs/ y archiva
openspec validate --all                       # valida que los artefactos son estructuralmente correctos
```

`openspec archive` mueve el change completado a `changes/archive/<fecha>-<nombre>/` y aplica los
deltas sobre `openspec/specs/<capability>/spec.md`. El hook `protect-files` bloquea ediciones
**manuales** a `openspec/specs/**`: solo `openspec archive`/`openspec-sync-specs` tocan ese árbol.

## Escenarios = contrato = tests (donde son testeables)

1. `propose`: el `product-manager` escribe escenarios testeables (o los marca explícitamente como verificación manual si el código no tiene entorno de test).
2. `red`: el `test-writer` convierte cada escenario testeable en un test que falla (skill `tdd-cycle`), solo dentro de `motherBoard/modules/control/`.
3. `green`/`refactor`: el `senior-developer` implementa hasta verde.
4. `verify`: el `qa-engineer` confirma que cada escenario testeable tiene test y pasa; los no testeables quedan como checklist manual.
5. `archive`: los deltas se fusionan; la spec viva queda como nuevo estado de verdad.

## Reglas

- **No edites `Firmware/openspec/specs/` a mano** (lo bloquea un hook): cambia vía deltas en el change y `openspec-archive-change`.
- Un escenario testeable sin test es un contrato sin verificar: no se archiva. Un escenario no testeable debe estar marcado como tal explícitamente, no simplemente omitido.
- Mantén las specs como única fuente de verdad; los tests son su materialización, no documentación duplicada.
