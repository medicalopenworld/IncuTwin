#!/usr/bin/env bash
# PostToolUse (Edit|Write): formatea el archivo C/C++ editado con clang-format.
# Recibe la ruta como argumento. Best-effort: nunca rompe el flujo (siempre
# exit 0); los diagnósticos van a stderr. Solo formatea el fichero tocado —
# NUNCA reformatea el árbol completo (motherBoard/Display_HMI son código
# legado sin .clang-format aplicado retroactivamente; un reformat masivo
# generaría un diff enorme sin valor).
set -uo pipefail

file_path="${1:-}"
[ -z "$file_path" ] && exit 0
[ -f "$file_path" ] || exit 0

case "$file_path" in
  *.c|*.h|*.cpp|*.cc|*.hpp|*.ino)
    clang-format -i "$file_path" >/dev/null 2>&1 || echo "clang-format: no aplicado a $file_path" >&2
    ;;
esac

exit 0
