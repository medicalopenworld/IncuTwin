---
paths:
  - "shared/**"
---

# Convenciones de Firmware/shared/

- `shared/include/{protocol.h, alarm_ids.h, control_types.h}` es consumido por **ambas** placas (`motherBoard` y `Display_HMI`) vía `lib_extra_dirs = ../shared` en sus respectivos `platformio.ini`. No es código de una sola placa.
- **Antes de cambiar la forma de un mensaje, un ID de alarma o un tipo de control**: revisa los dos consumidores (`grep` de uso en `motherBoard/src/` y `Display_HMI/src/`) — un cambio aquí que no se refleje en ambos lados rompe la sincronización descrita en `Firmware/PROTOCOL.md` de forma silenciosa (compila en ambas placas, falla en runtime).
- El build automático de esta capa (`Firmware/.claude/hooks/run-affected-tests.sh`) compila **ambas** placas cuando el diff toca `shared/**`, precisamente por este riesgo — no lo desactives ni lo saltes.
- Cambios en `shared/` se commitean con scope `(shared)` y, si son cruzados, documenta en el mensaje qué lado de cada placa se actualizó a la vez.
