#!/usr/bin/env bash
# require-retro.sh — Stop hook que BLINDA el paso 7 (learnings). Si la rama
# de trabajo cerró sub-tarea(s) de épica (marcó [x] en Firmware/docs/epics)
# pero NO registró nada en Firmware/docs/retro/, bloquea el cierre pidiendo
# el paso de learnings (un aprendizaje aplicado o un descarte motivado).
#
# No aplica en modo "oneshot". Tiene salvaguarda anti-bucle. Capa Firmware/
# (motherBoard/Display_HMI/shared) — independiente del require-retro.sh de
# SensorBoard_v2, que vigila sus propios docs/epics.
set -uo pipefail
cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0

input=$(cat 2>/dev/null || echo '{}')
active=$(printf '%s' "$input" | jq -r '.stop_hook_active // false' 2>/dev/null || echo false)

mode="$(cat .claude/.loop-mode 2>/dev/null | tr -d '[:space:]' || true)"
[ "$mode" = "oneshot" ] && exit 0

base="$(git rev-parse --verify --quiet dev >/dev/null 2>&1 && echo dev || echo HEAD~1)"
closed="$(git diff "$base"...HEAD -- docs/epics 2>/dev/null | grep -cE '^\+.*\[x\]' || true)"
[ "${closed:-0}" -eq 0 ] && exit 0

retro_touched="$(git diff --name-only "$base"...HEAD -- docs/retro 2>/dev/null | grep -c . || true)"
if [ "${retro_touched:-0}" -gt 0 ]; then
  exit 0
fi

if [ "$active" = "true" ]; then
  printf '{"systemMessage":"⚠ Paso 7 sin registro en Firmware/docs/retro/ tras varios intentos. Revisa manualmente."}\n'
  exit 0
fi

reason="PASO 7 OBLIGATORIO: esta rama marcó ${closed} checkbox(es) [x] en Firmware/docs/epics pero no \
registró nada en Firmware/docs/retro/. Antes de cerrar, aplica el paso de learnings (skill meta-self-\
improvement): repasa la sub-tarea sobre la EVIDENCIA (diff, reintentos, fricciones) y, según el umbral \
(segunda ocurrencia, coste observable, generalizable), o bien enruta un aprendizaje a su sitio (hook/rule/\
skill/agent de Firmware/.claude, Firmware/CLAUDE.md o docs) y regístralo en \
Firmware/docs/retro/<fecha>-<slug>.md, o bien anótalo ahí como descartado con motivo. No cierres sin dejar \
ese registro trazable."

if command -v jq >/dev/null 2>&1; then
  jq -nc --arg r "$reason" '{decision:"block", reason:$r}'
fi
exit 0
