---
paths:
  - "motherBoard/**"
  - "Display_HMI/**"
  - "shared/**"
---

# Reglas de seguridad (firmware / dispositivo médico)

- **Antes de tocar alarmas, comunicación serie, o el arranque/carga de firmware por USB: lee `Firmware/docs/known_issues.md` primero.** Ya documenta 5 fallos reales encontrados en producción (alarmas fantasma, inundación de UART, desincronización de idioma, propiedad del timer de fototerapia, carrera de bootload del CH340). No propongas una mitigación desde cero para un problema que ya tiene una causa raíz y una solución documentadas — y si vas a tocar una de esas zonas, confirma que tu cambio no reintroduce el bug ya corregido.
- **El protocolo motherBoard↔Display_HMI (`Firmware/PROTOCOL.md`) es texto ASCII por UART, sin CRC.** `CTRL,STATE`/`CTRL,TEL`/`CTRL,ALM`/`HMI,...` son líneas separadas por comas. Al parsear una línea entrante: valida el número de campos antes de indexarlos, valida que los campos numéricos (`alarmBitmask`, `hwNum`, etc.) sean parseables antes de usarlos, y trata una línea malformada/truncada como descarte silencioso — nunca como datos parciales válidos. No inventes una capa de integridad (CRC, checksum) que el protocolo real no tiene; si hace falta, es una decisión de diseño explícita (ADR), no un parche puntual.
- **Fail-safe, no fail-fast, en lecturas de sensor y control de actuadores.** Esto corre en una incubadora neonatal. Un fallo de lectura de sensor (I2C, timeout, NACK, valor `NaN` o fuera de rango físico) o una línea de protocolo corrupta debe reportarse como estado no disponible y dejar la tarea viva — nunca debe crashear, colgarse, ni (más crítico) dejar pasar un valor inválido a la lógica de PID/alarmas que controla un actuador (calefactor, humidificador). Descarta el valor y mantén el último estado seguro conocido o un fallback explícito.
- **Sin credenciales hardcodeadas.** `Credentials.h` (motherBoard) y equivalentes en Display_HMI están en `.gitignore` — nunca las metas en un commit ni las imprimas en logs.
- **No loguees líneas de protocolo crudas no validadas** si pueden contener datos sensibles o de más de un mensaje — sí es correcto loguear la línea descartada a nivel de error con su motivo (campos insuficientes, parseo numérico fallido).
- **Hooks que emiten JSON de control** (decision/permissionDecision/systemMessage): construye SIEMPRE la salida con `jq -nc --arg` (escape correcto); **nunca** con `printf`/`echo` interpolando variables. Un valor con comillas podría inyectar campos y secuestrar la decisión del hook.
