## MODIFIED Requirements

### Requirement: Claves opcionales de extensión [test-unity]
Para que ThingsBoard pueda enriquecer el estado sin cambiar firmware, el panel SHALL aceptar,
solo si están presentes y son válidas, estas claves adicionales, que prevalecen sobre el mapeo
anterior:

| Clave | Tipo / valores | Efecto |
|---|---|---|
| `thermo` | `off`, `heating`, `stable`, `alarm` | sustituye el `thermo` derivado de `heat` |
| `baby` | `none`, `in`, `parents`, `home` | sustituye el `baby` derivado de `state` |
| `awake` | bool | `awake` (por defecto false) |
| `skin` | entero 0..5 | tono de piel (por defecto el de Kconfig, 1) |
| `name` | string ≤ 23 bytes UTF-8 | nombre en la barra ("" = ninguno) |
| `country` | string ISO 3166-1 alfa-2 (`"CM"`; se aceptan minúsculas) | país de la incubadora (ausente = no mostrar) |
| `weight_g` | entero 0..9999 | peso (0 = no mostrar) |
| `age_d` | entero 0..9999 | días (ausente = no mostrar) |

Valores inválidos de una clave opcional SHALL ignorarse sin invalidar el mensaje.

#### Scenario: Canguro vía extensión
- **WHEN** llega `state: baby, baby: "parents"`
- **THEN** `baby = parents` y la pantalla muestra "Con sus papás"

#### Scenario: Extensión inválida
- **WHEN** llega `skin: 9`
- **THEN** el tono no cambia y el resto del mensaje se aplica

#### Scenario: País
- **WHEN** llega `country: "cm"`
- **THEN** el país es `CM`; con `country: "Camerun"` se ignora
