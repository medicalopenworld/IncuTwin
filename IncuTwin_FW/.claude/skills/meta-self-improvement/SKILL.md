---
name: meta-self-improvement
description: Protocolo de retro y automejora del framework Genesis para motherBoard/Display_HMI/shared. Usar en el stage final del loop para extraer aprendizajes y mejorar Firmware/.claude/ y Firmware/CLAUDE.md (hooks, rules, prompts de agentes, skills), o cuando una iteración revele fricción repetida que convenga sistematizar.
---

# Automejora del framework (capa Firmware/)

Genesis se automejora: cada iteración del loop termina dejando el framework mejor configurado. Lo
ejecuta el agente `retro-improver`. Serie de ADRs propia de esta capa en `Firmware/docs/adr/`
(independiente de la de `SensorBoard_v2/docs/adr/`) — sigue la convención `NNNN-slug.md` del skill
`git-flow`. El "paso 7" es el stage 11 del loop formal (ver tabla de stages en `loop-engineering`).

## Regla central: filtro de segunda ocurrencia

**No conviertas en convención un fallo de primera vez.** La primera ocurrencia puede ser
casualidad; commitear una regla por cada error ahoga las instrucciones en lecciones de un solo uso.
La convención nace en la **segunda ocurrencia del mismo patrón** — o si hay coste observable
(reintento, corrección manual, build roto) o es claramente generalizable. Lo demás se **registra
como descartado con motivo**, no se sistematiza.

## Pregunta de enrutado (decisiva)

Antes de elegir dónde aplicar una mejora, pregunta en orden:

1. **¿Debe ser imposible sin depender del contexto del modelo?** → **hook** (determinista).
2. ¿Es un procedimiento multi-paso repetido? → **skill**.
3. ¿Es una convención de una zona del repo (una placa concreta)? → **rule** (`paths:`).
4. ¿El agente lo necesita en TODA sesión y no lo infiere del código? → **CLAUDE.md** (conciso).
5. ¿Es decisión de diseño o hallazgo de investigación? → **docs/** (ADR/retro).
6. Si nada aplica → descartar (anotándolo).

## Protocolo

1. **Observa la iteración** (evidencia, no intuición): `Firmware/.claude/logs/loop.log`, el diff
   completo, qué stages necesitaron reintentos. ¿Qué prompt no disparó el agente correcto? ¿Qué
   regla faltó? ¿El build automático (`run-affected-tests.sh`) eligió mal la placa o el entorno?
2. **Registra aprendizajes** en `Firmware/docs/retro/<YYYY-MM-DD>-<feature>.md`.
3. **Aplica mejoras concretas**, eligiendo la frontera correcta:

| Síntoma observado                        | Mejora                                            | Dónde                                                  |
| ----------------------------------------- | -------------------------------------------------- | -------------------------------------------------------- |
| Un error que debería ser imposible       | Guardarraíl determinista                          | **hook** (`Firmware/.claude/hooks/` + `settings.json`)  |
| Convención ignorada en una placa         | Regla con `paths:`                                | **rule** (`Firmware/.claude/rules/`)                     |
| Procedimiento multi-paso repetido        | Empaquetar el procedimiento                       | **skill** (`Firmware/.claude/skills/`)                   |
| Hecho estable que falta en cada sesión   | Añadir (conciso)                                  | **Firmware/CLAUDE.md**                                   |
| Un agente no se activó o divagó          | Afinar `description` (disparo) o el system prompt | **agent** (`Firmware/.claude/agents/`)                   |

## Límites

- No edites `Firmware/openspec/specs/**` (protegido) ni relajes guardarraíles de seguridad.
- No toques `SensorBoard_v2/.claude/` ni su `openspec/` — es una instancia independiente.
- Toda mejora debe justificarse por algo observado en ESTA iteración, no especulativa.
- Mejoras arriesgadas → déjalas como propuesta anotada para validación humana.

## Mantenimiento del momentum

1. **`Firmware/ESTADO.md`**: épica/tarea activa, próximo paso inmediato, decisiones vigentes.
2. **`Firmware/docs/epics/`**: marca los checkboxes de la subtarea cerrada.
3. **Memoria de subagente** (`memory: project` en `retro-improver`/`product-manager`).

Protocolo al terminar una tarea: actualizar `Firmware/ESTADO.md` → marcar checkbox en `docs/epics/` → commit.

## Salida

Commit `chore(meta): retro + improve .claude` con: el retro escrito y la lista de cambios a `Firmware/.claude`/`CLAUDE.md` (archivo + qué + por qué).
