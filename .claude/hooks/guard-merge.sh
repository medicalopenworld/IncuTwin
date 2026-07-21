#!/usr/bin/env bash
# guard-merge.sh — PreToolUse (Bash) hook de los GATES HITL.
# SIEMPRE bloquea integraciones irreversibles sin aprobación humana marcada,
# sin importar el modo del loop: en firmware de un dispositivo médico
# (motherBoard/Display_HMI/shared), integrar (merge/tag) siempre pasa por
# revisión humana:
#   - git merge ... (hacia dev/main)
#   - git tag v<x.y.z> (release)
# El humano aprueba ⇒ el agente crea Firmware/.claude/.merge-approved
# (marca de un solo uso) ⇒ el merge pasa y la marca se consume.
set -uo pipefail
cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0

input=$(cat 2>/dev/null || echo '{}')
cmd=$(printf '%s' "$input" | jq -r '.tool_input.command // ""' 2>/dev/null || echo "")

case "$cmd" in
  *"git merge"*|*"git tag v"*) ;;
  *) exit 0 ;;
esac

approval=".claude/.merge-approved"
if [ -f "$approval" ]; then
  rm -f "$approval"   # marca de un solo uso: se consume
  exit 0
fi

deny_reason="GATE HITL: esta acción es irreversible (merge a dev/main o tag de release). En firmware médico \
(motherBoard/Display_HMI/shared) esto SIEMPRE requiere aprobación humana explícita, sin importar el modo del \
loop. PARA y pide al usuario su visto bueno explícito mostrándole el estado (tareas hechas, build 'pio run \
-e main' verde en las placas afectadas, diff resumido). Cuando el usuario apruebe, crea la marca de aprobación \
con 'printf ok > .claude/.merge-approved' y reintenta la acción (se consume en un solo uso). No \
mergees ni etiquetes sin esa aprobación."

jq -nc --arg r "$deny_reason" '{
  hookSpecificOutput: { hookEventName: "PreToolUse", permissionDecision: "deny", permissionDecisionReason: $r }
}'
exit 0
