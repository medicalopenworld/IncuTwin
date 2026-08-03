---
name: arch-embedded-layering
description: Convenciones de límites de módulo y principios SOLID-adyacentes que sí trasladan a C++17 para motherBoard/Display_HMI/shared. Usar al diseñar dónde vive la lógica de una feature nueva, al definir el header público de un módulo, o ante dudas sobre qué puede depender de qué entre las dos placas y shared/.
---

# Límites de módulo (motherBoard / Display_HMI / shared)

## La regla de oro

**`Firmware/shared/` es el único contrato formal entre placas.** Ninguna placa debe asumir la
estructura interna de la otra — todo lo que motherBoard y Display_HMI necesitan compartir (formato
de mensajes, IDs de alarma, tipos de control) vive en `shared/include/{protocol.h, alarm_ids.h,
control_types.h}` y se consume vía `lib_extra_dirs = ../shared`.

```
Firmware/
├─ shared/include/          ── contrato: protocol.h, alarm_ids.h, control_types.h
│                              (consumido por AMBAS placas — cambiarlo exige revisar ambos lados)
│
├─ motherBoard/src/
│   ├─ system/               ── init de hardware, buzzer, seguridad, EEPROM/Preferences
│   ├─ drivers/               ── periféricos concretos (BQ25730, INA3221, SHTC3, STS3X...)
│   ├─ hal/                   ── abstracción por revisión de hardware (hal_hw16/hal_hw17)
│   ├─ modules/
│   │   ├─ control/            ── PID, máquina de alarmas — el ÚNICO código con test nativo hoy
│   │   ├─ sensors/            ── lectura/interpretación de sensores
│   │   └─ comm/               ── protocolo serie hacia Display_HMI (PROTOCOL.md)
│   ├─ state/                  ── estado global de la placa
│   ├─ tasks/                  ── tareas FreeRTOS (CommTask, GPRS, Wifi_OTA, DriveUpload)
│   └─ legacy/                 ── UI on-board antigua, superada por Display_HMI
│
└─ Display_HMI/src/
    ├─ ui/                     ── LVGL (parte generada por SquareLine Studio)
    ├─ drivers/, hal/          ── igual que motherBoard, propio de esta placa
    ├─ modules/audio/          ── alarmas sonoras
    ├─ state/                  ── estado global de la UI
    └─ tasks/                  ── AudioManager, CommTask, UITask, Wifi_OTA
```

| Módulo                    | Contiene                                                  | NO debe hacer                                                       |
| --------------------------- | ------------------------------------------------------------ | ----------------------------------------------------------------------- |
| `shared/`                    | el contrato de mensajes/tipos entre placas                  | contener lógica de negocio de ninguna placa concreta                    |
| `modules/control` (motherBoard) | PID, máquina de alarmas — puro, testeable en host       | parsear el protocolo serie ni tocar hardware directamente               |
| `modules/comm`                | serializar/deserializar el protocolo (`PROTOCOL.md`)        | decidir lógica de control (eso vive en `modules/control`)                |
| `ui/` (Display_HMI)           | presentación LVGL                                           | lógica de negocio de alarmas/control (esa vive en `modules/`, no en la UI) |
| `legacy/` (motherBoard)       | UI on-board antigua                                          | recibir inversión de esfuerzo nueva sin confirmación explícita          |

## El header público ES la interfaz

En C++17 sin un contenedor de DI, la interfaz real de un módulo es lo que expone en su `.h`:

- Lo declarado en el header público es el contrato que otros módulos pueden usar.
- Los detalles internos (miembros privados, funciones estáticas de traducción) no se exponen.
- Un módulo que necesita algo de otro incluye su header público, nunca accede a su implementación interna por atajo.

## FreeRTOS como mecanismo de composición

- **Tareas** (`xTaskCreate`, vía el core Arduino-ESP32) son la unidad de ejecución independiente.
- **Colas** y **semáforos** son el "cableado" entre una ISR/callback productor y una tarea consumidora.
- **Ninguna lógica en una ISR/callback** más allá del hand-off — la lógica vive siempre en la tarea.

## Qué de SOLID sí traslada

- **S (responsabilidad única)**: un módulo = una responsabilidad. `modules/control` no parsea protocolo; `modules/comm` no decide alarmas.
- **D (depender de abstracciones)**: un módulo depende del **header público** de otro, nunca de su implementación interna — y ninguna placa depende de la implementación interna de la otra, solo de `shared/`.
- El resto de SOLID no tiene traducción forzada aquí — no se inventa una analogía artificial.

## Olores a evitar

- Asumir el layout interno de la otra placa en vez de pasar por `shared/`.
- Lógica de negocio (interpretación de alarmas, cálculo de PID) dentro de `modules/comm` o de `ui/`.
- Trabajo pesado o bloqueante dentro de una ISR/callback.
- Invertir esfuerzo nuevo en `legacy/` (motherBoard) sin confirmar que sigue siendo necesario.
