## ADDED Requirements

### Requirement: Estados del gesto [test-unity]
El gesto "Coge mi mano" SHALL seguir estos estados, con `T_min = 60 s`, `T_gracia = 3 s`,
`T_listo = 20 s` y `T_gracias = 4 s` (opciones de compilación):

| Estado | Entra cuando | Sale a |
|---|---|---|
| reposo | arranque; fin de cualquier otro estado | listo (botón), sosteniendo (dedo en el bebé) |
| listo | se pulsa el botón | sosteniendo (dedo en el bebé); reposo tras `T_listo` sin tocar |
| sosteniendo | dedo en el bebé | gracia (se levanta antes de `T_min`); completado (acumula `T_min`) |
| gracia | se levanta el dedo antes de `T_min` | sosteniendo (vuelve el dedo, conserva el progreso); reposo (pasan `T_gracia`, progreso a 0) |
| completado | se acumulan `T_min` de dedo | sigue mientras haya dedo, sin límite, y sigue sumando dedo (vueltas extra del anillo); al levantarlo muestra el agradecimiento `T_gracias` y pasa a reposo; si vuelve el dedo durante el agradecimiento, vuelve a completado y sigue sumando desde donde estaba |

El progreso SHALL contar solo el tiempo con el dedo puesto. Si la vista BEBÉ deja de mostrarse,
el gesto SHALL volver a reposo sin enviar nada.

#### Scenario: Gesto completo con una pausa
- **WHEN** se pulsa el botón, se pone el dedo 35 s, se levanta 2 s y se vuelve a poner 25 s
- **THEN** a los 35 + 25 = 60 s de dedo el gesto pasa a completado

#### Scenario: Pausa demasiado larga
- **WHEN** en sosteniendo con 40 s acumulados se levanta el dedo 3 s
- **THEN** el gesto vuelve a reposo con el progreso a 0 y no se envía nada

#### Scenario: Listo sin tocar
- **WHEN** se pulsa el botón y no se toca al bebé en 20 s
- **THEN** el gesto vuelve a reposo

#### Scenario: Seguir después de completar
- **WHEN** el gesto completa a los 60 s y el dedo sigue 3 min más
- **THEN** el estado sigue en completado durante los 3 min y no se envía un segundo aviso

#### Scenario: Vueltas extra
- **WHEN** se mantiene el dedo 150 s seguidos
- **THEN** el tiempo de dedo acumulado es 150 s (dos vueltas completas y media de la tercera),
  se envía un solo aviso y, al levantar el dedo, el acumulado se queda quieto durante el
  agradecimiento

#### Scenario: Tocar directamente
- **WHEN** en reposo se pone el dedo sobre el bebé sin pulsar el botón
- **THEN** el gesto pasa a sosteniendo

### Requirement: Aviso a la IncuNest [test-unity]
Al pasar a completado, el panel SHALL publicar una única vez en `incutwin/<client_id>/hand`,
QoS 1, no retenido, el JSON `{"hand_hold":1,"incubator_id":"<id emparejado>","hold_s":<T_min>}`.
NO SHALL publicar en modo demo, sin sesión con el broker ni sin incubadora emparejada; en esos
casos el gesto se completa igual en pantalla.

#### Scenario: Aviso normal
- **WHEN** el panel emparejado con `353` y conectado completa el gesto
- **THEN** se publica `{"hand_hold":1,"incubator_id":"353","hold_s":60}` en `incutwin/<id>/hand`

#### Scenario: En demo
- **WHEN** se completa el gesto en modo demo
- **THEN** la pantalla agradece igual y no se publica nada

### Requirement: Espera entre avisos [test-unity]
Entre dos avisos publicados SHALL pasar al menos `T_espera` segundos (opción de compilación,
por defecto 0 = sin espera). Un gesto completado dentro de la espera SHALL mostrarse igual y no
publicar. La espera SHALL medirse desde el último aviso publicado, no desde el último gesto.

#### Scenario: Espera a cero
- **WHEN** `T_espera = 0` y se completan dos gestos seguidos
- **THEN** se publican dos avisos

#### Scenario: Espera de 30 min
- **WHEN** `T_espera = 1800`, se publica un aviso y 10 min después se completa otro gesto
- **THEN** el segundo no se publica; uno completado a los 31 min sí
