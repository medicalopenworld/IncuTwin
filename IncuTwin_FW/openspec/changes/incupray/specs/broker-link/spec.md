## ADDED Requirements

### Requirement: Topics de la IncuPray [manual]
Al conectar, el panel SHALL suscribirse a `incupray/next` (retenido), `incupray/live` e
`incupray/summary` con QoS 1, y SHALL poder publicar en `incutwin/<id>/incupray`. Un payload que no
valida (JSON roto, campos que faltan) SHALL ignorarse sin afectar al resto del enlace. Fuera de la
ventana de la IncuPray, `incupray/live` NO SHALL generar trabajo en la UI.

#### Scenario: Horario retenido al conectar
- **WHEN** el panel conecta y hay un `incupray/next` retenido para el domingo
- **THEN** el panel conoce la próxima IncuPray sin esperar a otro mensaje
