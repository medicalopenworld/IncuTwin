## REMOVED Requirements

### Requirement: Agarra mi mano [manual]
**Reason**: sustituido por el gesto sostenido de un minuto con aviso a la IncuNest (capability
`hold-my-hand` y requisito "Coge mi mano en pantalla").
**Migration**: el botón pasa a llamarse "Coge mi mano"; tocar al bebé ya no lo despierta, sino
que empieza el gesto.

## ADDED Requirements

### Requirement: Coge mi mano en pantalla [manual]
En la vista BEBÉ, el gesto (ver `hold-my-hand`) SHALL presentarse así:

| Estado | Bebé | Fila inferior (sustituye a iconos y botón) | Barra de estado |
|---|---|---|---|
| reposo | estado real | iconos + botón "Coge mi mano" / "Hold my hand" | la normal |
| listo | ojos abiertos; diana coral pulsando sobre el pecho | "Pon tu dedo sobre el bebé" / "Put your finger on the baby" | "Te está esperando" / "Waiting for you" (coral) |
| sosteniendo | ojos cerrados; anillo coral que se llena en `T_min`; corazones que suben | "Le estás dando la mano..." hasta la mitad, luego "Ya casi... siente tu calor" / "You're holding their hand...", "Almost there... feel your warmth" | "No sueltes, se está calmando" / "Don't let go, they're calming down" (coral) |
| gracia | ojos abiertos; anillo atenuado y quieto | "¿Sigues ahí? Vuelve a ponerlo" / "Still there? Put it back" (coral) | "Te espera N s más" / "Waiting N s more", N = segundos que quedan (coral) |
| completado | ojos cerrados; anillo lleno; fondo coral suave `0xFBEDEA`; corazones | "Gracias por tu calorcito" / "Thank you for your warmth" | "Quédate todo lo que quieras" / "Stay as long as you like" (verde) |

El anillo SHALL rodear el halo (diámetro ≈ 196 px, grosor 6 px, fondo `0xF5C4B3`, relleno
`COL_CORAL`) y avanzar de forma continua (refresco ≤ 200 ms). Nunca se muestra un número de
segundos salvo en la barra de estado durante la gracia.

#### Scenario: Del botón al agradecimiento
- **WHEN** con el bebé durmiendo se pulsa "Coge mi mano" y luego se mantiene el dedo 60 s
- **THEN** al pulsar el botón el bebé abre los ojos y aparece la diana; al tocarle cierra los ojos
  y el anillo empieza a llenarse; a los 60 s el fondo se vuelve coral suave y aparece "Gracias por
  tu calorcito"

#### Scenario: Soltar un momento
- **WHEN** en sosteniendo se levanta el dedo 3 s
- **THEN** el bebé abre los ojos, el anillo se queda quieto, la barra dice "Te espera 7 s más" y al
  volver el dedo el anillo sigue desde donde estaba

#### Scenario: Fin del gesto
- **WHEN** en completado se levanta el dedo
- **THEN** el agradecimiento se ve 4 s y después vuelven los iconos, el botón, el estado real de
  los ojos y la barra de estado normal

### Requirement: Etiqueta con el nombre [manual]
En la vista BEBÉ, si el estado trae `name` no vacío, SHALL verse una etiqueta blanca con borde
coral y el nombre en negrita, centrada sobre la parte baja de la manta (por encima de la fila de
iconos), en todos los estados del gesto. Sin `name` la etiqueta NO SHALL verse (no se pone texto
de sustitución). Un nombre largo SHALL truncarse con puntos suspensivos dentro de 200 px.

#### Scenario: Con nombre
- **WHEN** `show_baby` con `name = "Lucía"`
- **THEN** se ve la etiqueta "Lucía" sobre la manta

#### Scenario: Sin nombre
- **WHEN** `show_baby` y el estado no trae `name`
- **THEN** no se ve ninguna etiqueta
