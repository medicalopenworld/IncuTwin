#!/usr/bin/env bash
# SessionStart (startup|clear|compact): inyecta contexto del proyecto para
# orientar la sesión y sobrevivir a la compactación. Capa Firmware/ (motherBoard
# /Display_HMI/shared) — independiente de SensorBoard_v2, que gestiona su
# propio contexto en su propio .claude/.
set -uo pipefail

cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0

branch="$(git branch --show-current 2>/dev/null || echo '(sin git)')"
dirty="$(git status --short -- motherBoard Display_HMI shared 2>/dev/null | wc -l | tr -d ' ')"
active_change="$(openspec list --json 2>/dev/null | jq -r '[.changes[].name][0:3] | join(",")' 2>/dev/null || true)"
momentum="$(awk '/^## Épica activa/{f=1;next} /^## /{f=0} f && NF' ESTADO.md 2>/dev/null | grep -m1 'EPIC' | sed 's/^[[:space:]-]*//' || true)"

ctx="Genesis (capa motherBoard+Display_HMI+shared) · rama actual: ${branch} · cambios sin commitear en esas placas: ${dirty}."
[ -n "$momentum" ] && ctx="${ctx} MOMENTUM (ver ESTADO.md): ${momentum}."
[ -n "$active_change" ] && ctx="${ctx} OpenSpec changes activos: ${active_change}."
ctx="${ctx} SensorBoard_v2/ tiene su PROPIO framework independiente (no gobernado por este .claude/). Gitflow: feat/* (producto) y meta/* (capa agéntica) → dev (merge --no-ff), release/* → main."

mode="$(cat .claude/.loop-mode 2>/dev/null | tr -d '[:space:]' || true)"
if [ -n "$mode" ]; then
  ctx="${ctx} MODO DEL LOOP activo: ${mode} (ver skill loop-modes; cambiar con /loop-mode)."
else
  ctx="${ctx} MODO DEL LOOP no fijado: ANTES de ejecutar la primera tarea, pregunta al usuario qué modalidad quiere y fíjala con /loop-mode. Detalle en el skill loop-modes."
fi

jq -n --arg c "$ctx" '{
  hookSpecificOutput: {
    hookEventName: "SessionStart",
    additionalContext: $c
  }
}'
exit 0
