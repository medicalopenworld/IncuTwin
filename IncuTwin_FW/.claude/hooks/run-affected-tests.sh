#!/usr/bin/env bash
# Stop: build automático board-aware. Detecta que placa(s) toca el diff sin
# commitear (comparado con HEAD, staged+unstaged) por prefijo de ruta dentro
# de Firmware/, y compila SOLO esas placas con PlatformIO. NUNCA flashea ni
# abre el monitor serie (pio run -t upload / pio device monitor) — eso queda
# siempre manual, requiere hardware físico conectado.
#
#   motherBoard/**  -> pio run -e main
#                      + pio test -e native si toca modules/control/{alarm_machine,pid_wrapper}.cpp
#                        (ese entorno corre en el host, sin hardware: seguro de automatizar)
#   Display_HMI/**  -> pio run -e main
#   shared/**       -> AMBAS placas (un header compartido puede romper cualquiera en silencio)
#   SensorBoard_v2/** -> se ignora explícitamente: esa placa tiene su propio
#                        Stop hook en su propio .claude/ (ESP-IDF/idf.py); no
#                        se duplica ni se asume su toolchain desde esta capa.
#
# Respeta stop_hook_active para no chocar con el cap de bloqueos.
set -uo pipefail

input="$(cat)"
active="$(printf '%s' "$input" | jq -r '.stop_hook_active // false')"
[ "$active" = "true" ] && exit 0

cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0

changed="$(git diff --name-only HEAD -- motherBoard Display_HMI shared 2>/dev/null || true)"
[ -z "$changed" ] && exit 0

touches() { printf '%s\n' "$changed" | grep -q "^$1"; }

run_build() {
  # $1 = directorio de la placa (motherBoard | Display_HMI), $2 = nombre para el mensaje
  local dir="$1" label="$2"
  if [ ! -f "$dir/platformio.ini" ]; then
    return 0   # placa sin scaffold todavía: nada que compilar
  fi
  if ! out="$(cd "$dir" && pio run -e main 2>&1)"; then
    tail_out="$(printf '%s' "$out" | tail -25)"
    jq -n --arg r "pio run -e main ($label) en rojo; corrige antes de cerrar:
$tail_out" '{decision:"block", reason:$r}'
    exit 0
  fi
}

run_native_test() {
  if ! out="$(cd motherBoard && pio test -e native 2>&1)"; then
    tail_out="$(printf '%s' "$out" | tail -25)"
    jq -n --arg r "pio test -e native (motherBoard) en rojo; corrige antes de cerrar:
$tail_out" '{decision:"block", reason:$r}'
    exit 0
  fi
}

need_mb=false
need_hmi=false
need_native=false

touches '^motherBoard/' && need_mb=true
touches '^Display_HMI/' && need_hmi=true
touches '^shared/' && { need_mb=true; need_hmi=true; }
touches '^motherBoard/src/modules/control/alarm_machine\.cpp' && need_native=true
touches '^motherBoard/src/modules/control/pid_wrapper\.cpp' && need_native=true

[ "$need_mb" = true ] && run_build motherBoard "motherBoard"
[ "$need_hmi" = true ] && run_build Display_HMI "Display_HMI"
[ "$need_native" = true ] && run_native_test

exit 0
