---
paths:
  - "motherBoard/**"
  - "Display_HMI/**"
  - "shared/**"
---

# Reglas de testing

- **TDD estricto solo donde hay entorno de test real**: hoy, únicamente `motherBoard`'s `[env:native]` (Unity, host, sin hardware), cubriendo `modules/control/alarm_machine.cpp` y `pid_wrapper.cpp`. Ahí: test en rojo antes de implementar, verificado con `pio test -e native` (automatizable, se ejecuta en el Stop hook).
- **Donde no hay entorno de test** (resto de `motherBoard`, todo `Display_HMI`): TDD no es exigible todavía. La verificación es manual — documenta explícitamente qué se compiló (`pio run -e main`) y qué se probó en hardware real antes de dar el cambio por cerrado. No finjas cobertura de test que no existe.
- Los tests de `motherBoard` viven en `motherBoard/test/test_<área>/test_<área>.cpp` (ya establecido: `test_alarms/`, `test_pid/`), agrupados por área funcional.
- Nombres de test descriptivos del comportamiento esperado, no de la implementación interna.
- Ciclo real de ejecución en `motherBoard`:
  1. `pio run -e main` — compila la placa completa (Arduino + libs). Automatizable, es lo que ejecuta el Stop hook.
  2. `pio test -e native` — compila y corre los tests Unity en el host. Automatizable (sin hardware), se ejecuta en el Stop hook cuando el diff toca `alarm_machine.cpp`/`pid_wrapper.cpp`.
  3. Flashear/monitorizar hardware real (`pio run -e main -t upload`, `pio device monitor`) — **siempre manual**, nunca en un hook.
- Si se amplía el `build_src_filter` de `[env:native]` para cubrir más lógica testeable (p. ej. extraer más de `modules/control/`), la implementación debe ir acompañada de tests Unity nuevos en el mismo commit.
- No bajes la cobertura de escenarios para "pasar" el stage verify; si falta un caso, añade el test en vez de omitirlo.
