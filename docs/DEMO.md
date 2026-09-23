# Demo funcional IncuTwin con ThingsBoard

Incubadora real **IncuNest-1_2** (SN 1, FW `dev` 18.49) → rule chain
`IncuNest_IncuTwin_state` → panel físico **ITW-DEEF3C** + visor web
(hace de app del padrino) ← "Coge mi mano" (panel o visor).

## Puesta en marcha

```sh
export TB_URL=https://mon.medicalopenworld.org TB_API_KEY=...   # nunca en el repo
python tb/tb_apply_incutwin_state.py --enable IncuNest-1_2        # idempotente
python tb/tb_apply_hand_hold.py --map 1=IncuNest-1_2              # idempotente
python tools/demo_viewer.py --device IncuNest-1_2 --sn 1          # http://localhost:8765
pio run -e crowpanel_advance_28 -t upload --upload-port COM10     # necesita include/secrets.h
```

Emparejado: relación `IncuNest-1_2 —MirroredBy→ ITW-DEEF3C` (ya creada).
Si el panel quedó ligado a un device que no es del tenant, se envía
`tb-forget` por el puerto serie (115200): borra el token, conserva la WiFi
y se re-provisiona con el perfil `IncuTwin`.

## Guion (5 min)

| # | En la IncuNest (HMI) | Panel | Visor | Evento |
|---|---|---|---|---|
| 1 | En espera | Incubadora vacía | "La incubadora está esperando" | — |
| 2 | Asistente del bebé y encender calor | Aparece el bebé, halo naranja | "Tu bebé ha llegado…" | `baby_in` |
| 3 | Temperatura ±0,5 °C durante 5 min | Halo tranquilo | "ya está calentito" | `treatment_changed` |
| 4 | Encender fototerapia | Halo azul | "le dan luz azul…" | `treatment_changed` |
| 5 | Pulsar "Coge mi mano" en el panel o el visor | — | "Alguien le ha cogido la mano" | `hand_hold_until` en 1_2 |
| 6 | Canguro en el HMI | "Con sus papás" + fanfarria | "Está con sus papás" | `baby_parents` |
| 7 | Volver a encender calor | Bebé de vuelta | "Ha vuelto… tras N min" | `baby_back` |
| 8 | Alta con desenlace "sobrevive" | "¡Ya está en casa!" | "¡Se ha ido a casa! 🎉" | `baby_out` (`outcome: home`) |

Los cambios de tratamiento tienen un debounce de 30 s, y "estable" exige
5 min dentro de ±0,5 °C (`itw_state_machine.js`, `cfg`). Para ensayar sin
la incubadora, en el panel funciona el modo demo del botón BOOT.

## Dónde mirar si algo falla

- Estado calculado: server attrs `itw_state`, `itw_baby_state` de IncuNest-1_2.
- Eventos: telemetría `itw_event` (TTL 30 d); mano: `itw_hand_hold` /
  `itw_hand_hold_rejected` (`reason`: `no_baby`, `not_enabled`).
- Errores de nodos: TB → Rule chains → nodo → Events → Errors.
- Panel: log serie `[tb] …` y `[ui] show_baby=… baby=…`.

## Pendiente fuera de la demo

TLS 8883 en el panel; usuario TB de solo lectura para el visor; un device
por SN en lugar de `itw_sn_map`; `ChangeOriginator RELATED` solo alcanza al
primer panel emparejado (con varios padrinos hay que iterar las
relaciones); webhook a Firebase sin conectar.
