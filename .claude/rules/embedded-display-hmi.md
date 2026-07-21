---
paths:
  - "Display_HMI/**"
---

# Convenciones de Display_HMI (PlatformIO + Arduino, C++17, LVGL)

- **Entornos reales** (`Display_HMI/platformio.ini`): `main` (`default_envs`, build de producción) y `crash_test_hmi` (variante de diagnóstico con `CRASH_TEST_HMI=1`). No existe entorno de test (`test/` es un placeholder de PlatformIO sin ficheros) — no lo asumas ni lo inventes en este bootstrap; si una tarea futura lo requiere, es una propuesta explícita, no una sorpresa dentro de otro cambio.
- **Sin TDD estricto todavía**: la verificación es manual (`pio run -e main` + prueba visual/táctil en el CrowPanel real). Documenta explícitamente qué se probó a mano en el commit/PR cuando el cambio afecta a `ui/`, `tasks/UITask.cpp` o `modules/audio/`.
- **`display_config.h` es la única fuente de verdad de los pines RGB del panel.** Ya hubo un bug real (v2.0.0, documentado en el README del proyecto) por configuraciones de pines duplicadas/conflictivas que causaban pantalla en blanco o parpadeo. Nunca definas el pinout RGB en un segundo sitio "por comodidad" — siempre referencia ese fichero.
- **Aviso de build en Windows**: rutas largas pueden hacer fallar la compilación (límite de 260 caracteres); el README documenta el workaround (`core_dir = C:\pio`). Si un build falla con un error de ruta, es casi seguro esto, no un error de código.
- **Estructura real**: `ui/` (assets LVGL + `ElementsCreation.cpp`/`ui_helpers.c`, generado en parte por SquareLine Studio — no reescribas `ui_helpers.c` a mano si viene de ahí), `tasks/` (AudioManager, CommTask, UITask, Wifi_OTA — FreeRTOS), `drivers/`, `hal/`, `modules/audio/`, `state/`. No hay `system/` ni `legacy/` aquí (eso es de `motherBoard`).
- **`tasks/CommTask.cpp`** implementa el lado HMI del protocolo serie (`Firmware/PROTOCOL.md`) — ver `security.md` para las reglas de parseo de líneas no confiables.
- **`Firmware/shared/`** se consume igual que en `motherBoard` vía `lib_extra_dirs = ../shared` — ver `embedded-shared.md` antes de tocarlo.
