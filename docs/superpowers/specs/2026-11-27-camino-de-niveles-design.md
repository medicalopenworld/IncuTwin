# «Tu camino»: los niveles de afecto y lo que da cada uno

Fecha: 27-11-2026 · Estado: aprobado por Pablo (27-nov-2026) · Base: `feat/acompanar` en fd29f33
(niveles de afecto fase 1, PR #10)

## Para qué

Quien acompaña tiene que saber qué le espera al subir de nivel, para tener ganas de seguir. Hoy,
al tocar la tarjeta del nivel en Corazones solo sale cómo se hace lo que ya tiene abierto.

## Qué se hace

Al tocar la tarjeta del nivel en Corazones se abre «Tu camino», con el estilo «de bolsillo», en
lugar del diálogo actual. Es una fila por nivel:

```
 Tu camino                                   ✕
 ✔ Manita        0  Pulsa "Coge mi mano" y pon tu dedo
 ✔ Caricia      10  Si le acaricias, sonríe
 ▶ Cosquillitas 30  Acaríciale rápido y se ríe
                    Te faltan 18 corazones
 ○ Abrazo       75  Abrázala y le das la mano sin tocar la pantalla
                    Próximamente
 ○ Achuchón    150  Al pecho, sientes su latido
                    Próximamente
```

- Estado de cada nivel: conseguido, el siguiente (el primero no conseguido, con «Te faltan N
  corazones») o pendiente. Con el nivel más alto conseguido no hay «siguiente».
- Los niveles por encima de `RW_LEVEL_LAST_OPEN` (hoy Abrazo y Achuchón) llevan «Próximamente»:
  todavía no abren nada (fase 2, con la placa del acelerómetro).
- Los textos de lo que da cada nivel sirven también de «cómo se hace» para lo ya conseguido.
- Si no cabe en la pantalla, se desplaza; nunca se corta texto.
- En la demo, el camino es el de la demo (su nivel).

Textos (ES / EN):

| Nivel | Lo que da |
|---|---|
| Manita | Pulsa "Coge mi mano" y pon tu dedo / Tap "Hold my hand" and put your finger |
| Caricia | Si le acaricias, sonríe / Stroke the baby and they smile |
| Cosquillitas | Acaríciale rápido y se ríe / Stroke quickly and they laugh |
| Abrazo | Abrázala y le das la mano sin tocar la pantalla / Hug your IncuTwin to hold their hand without touching the screen |
| Achuchón | Al pecho, sientes su latido / Hold it to your chest and feel their heartbeat |

Título «Tu camino» / «Your path»; «Te faltan %u corazones» / «%u hearts to go»; «Próximamente» /
«Coming soon».

## Cambio en la fase 2 (Pablo, 27-nov-2026)

Sustituye lo que decía `2026-11-27-niveles-de-afecto-design.md` para Abrazo y Achuchón:

- **Abrazo (75):** abrazar la IncuTwin hace un «Coge mi mano» sin tocar la pantalla. Como es un
  «Coge mi mano» de verdad, da corazones y avisa a la IncuNest: un abrazo reconocido por error
  mandaría un aviso falso, así que el reconocimiento tendrá que ser estricto (se diseña en la fase 2).
- **Achuchón (150):** IncuTwin al pecho y el motor late con el corazón del bebé. Mecer para
  dormirle ya no forma parte del plan.

## Fuera

- Nada de la fase 2 se construye aquí: solo se anuncia con «Próximamente».
- No cambian umbrales, nombres ni la tarjeta de subida de nivel.

## Enfoques

Uno solo razonable: reutilizar el diálogo que ya abre la tarjeta y llenarlo con las filas. Una
pantalla nueva sería más trabajo para cinco filas; ponerlo fijo en Corazones alargaría la lista.
