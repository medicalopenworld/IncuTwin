---
name: tdd-cycle
description: El ciclo TDD red-green-refactor con Unity/PlatformIO en motherBoard/Display_HMI. Úsalo antes de escribir código de producción, y cuando haya dudas sobre dónde colocar tests, qué comando usar o si el código es testeable en host o exige verificación manual.
---

# Ciclo TDD (PlatformIO + Unity nativo, capa motherBoard/Display_HMI)

## TDD graduado, no uniforme

A diferencia de un stack con test runner universal, aquí el TDD estricto solo es exigible donde
existe un entorno de test real:

- **`motherBoard` `[env:native]`** (host, sin hardware, Unity): hoy cubre `modules/control/
  alarm_machine.cpp` y `pid_wrapper.cpp` (`build_src_filter` del entorno). **Aquí sí**: red → green
  → refactor estricto.
- **Resto de `motherBoard`** (`drivers/`, `system/`, `hal/`, `tasks/`, `legacy/`) y **todo
  `Display_HMI`**: sin entorno de test configurado. La "verificación" es compilar (`pio run -e
  main`) + probar en hardware real, documentado explícitamente — no se finge un test que no existe,
  ni se propone un mock de LVGL/hardware como sustituto (un mock de UART/I2C/LVGL no detecta un
  timing real roto).

## Red → Green → Refactor (donde aplica: motherBoard/modules/control)

1. **Red**: escribe un `TEST_CASE` en `motherBoard/test/test_<área>/test_<área>.cpp` que describa el
   comportamiento deseado y falle — por compilación (símbolo que aún no existe) o por aserción.
   Confirma que falla por la razón correcta antes de tocar producción.
2. **Green**: el mínimo código en `modules/control/` para pasar el test. Nada de adelantar
   funcionalidad sin test.
3. **Refactor**: con los tests en verde, mejora nombres/duplicación/límites de módulo (ver skill
   `arch-embedded-layering`) y vuelve a correr `pio test -e native`.

## Dónde y con qué

| Tipo de código                                                    | Ubicación del test                                    | Cómo se verifica                                                                                     |
| -------------------------------------------------------------------- | -------------------------------------------------------- | --------------------------------------------------------------------------------------------------------- |
| Lógica pura de control (`alarm_machine`, `pid_wrapper`)              | `motherBoard/test/test_<área>/test_<área>.cpp`         | `pio test -e native` (host, sin hardware, automatizable)                                                  |
| Resto de `motherBoard` (drivers I2C, HAL, tareas, comm)              | sin test unitario                                        | `pio run -e main` (compile-only) + prueba manual en hardware real                                         |
| `Display_HMI` (LVGL, audio, comm)                                    | sin test unitario                                        | `pio run -e main` + prueba visual/táctil manual en el CrowPanel real                                      |

## Comandos

```bash
# motherBoard
pio run -e main                       # compila la placa completa — gate automatizable
pio test -e native                    # compila y corre los tests Unity en el host — automatizable
pio run -e main -t upload             # flashea — SIEMPRE manual
pio device monitor                    # monitor serie — SIEMPRE manual

# Display_HMI
pio run -e main                       # compila — gate automatizable
pio run -e main -t upload             # flashea — SIEMPRE manual
```

## Convenciones

- Tests Unity de `motherBoard` agrupados por área funcional en `motherBoard/test/test_<área>/`, ya establecido (`test_alarms/`, `test_pid/`).
- Nombres de `TEST_CASE` descriptivos del comportamiento, en español.
- Ningún test de hardware/UI real se ejecuta en un hook automatizado (ver `run-affected-tests.sh`): el desarrollador prueba en el dispositivo él mismo cuando el cambio lo requiere.
- No hay umbral numérico de cobertura: la evidencia de "suficientemente testeado" es que cada escenario testeable de la spec (ver `spec-driven-development`) tiene un `TEST_CASE` que pasa, o una verificación manual documentada explícitamente si el código no es testeable hoy.
- Si extiendes el `build_src_filter` de `[env:native]` para cubrir más lógica, la implementación va acompañada de tests Unity nuevos en el mismo commit.
