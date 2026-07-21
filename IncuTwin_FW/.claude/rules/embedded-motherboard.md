---
paths:
  - "motherBoard/**"
---

# Convenciones de motherBoard (PlatformIO + Arduino, C++17)

- **Entornos reales** (`motherBoard/platformio.ini`): `main` (alias estable de la revisión de hardware vigente — hoy `extends = env:IncuNest_V17`; usar siempre este nombre en builds/hooks, nunca un número de versión fijo), `IncuNest_V16`/`IncuNest_V17`/futuras revisiones numeradas (para testear una revisión de hardware concreta), `native` (entorno host, sin hardware, Unity — `test_framework = unity`).
- **`[env:native]` es donde el TDD estricto aplica de verdad**: hoy cubre `modules/control/alarm_machine.cpp` y `pid_wrapper.cpp` (`build_src_filter` del entorno). Si extiendes esos dos ficheros, escribe primero el test en `test/test_alarms/`/`test/test_pid/` y verifica con `pio test -e native` (corre en el host, sin hardware — seguro de automatizar). El resto del árbol (`drivers/`, `system/`, `legacy/`, `hal/`, `tasks/`) no tiene entorno de test: verificación manual documentada (`pio run -e main` + prueba en placa real), igual que se documentaría un checklist de hardware en cualquier firmware embebido.
- **Capas por revisión de hardware**: `hal/hal_hw16.cpp` y `hal/hal_hw17.cpp` se excluyen mutuamente por `build_src_filter` según el entorno. Un cambio en `hal/` casi siempre necesita tocar ambos ficheros o justificar por qué no.
- **`legacy/`** contiene la UI on-board antigua, superada por `Display_HMI` — no es el código activo de interfaz; confirma con Pablo antes de invertir esfuerzo ahí salvo que se indique explícitamente.
- **`modules/{comm,control,sensors}/`** son los límites de responsabilidad reales: `control/` (PID, máquina de alarmas), `sensors/` (lectura), `comm/` (protocolo serie hacia Display_HMI, ver `Firmware/PROTOCOL.md`). No mezcles lógica de interpretación de sensor con el parseo del protocolo serie en el mismo fichero.
- **`Firmware/shared/`** (`protocol.h`, `alarm_ids.h`, `control_types.h`) se consume vía `lib_extra_dirs = ../shared` — ver `embedded-shared.md` antes de tocarlo.
