---
name: loop-mode
description: Fija o muestra la modalidad de ejecución del loop (auto / human / oneshot) para motherBoard/Display_HMI/shared. Usar cuando el usuario escribe /loop-mode, pide cambiar de modalidad, o quiere saber el modo activo. Detalle en el skill loop-modes.
---

# /loop-mode — fijar la modalidad

Gestiona el fichero `.claude/.loop-mode` (gitignored) de esta capa (Firmware/, no confundir con el de
`SensorBoard_v2/.claude/.loop-mode`, que es independiente). Las modalidades están en el skill `loop-modes`.

## Acción

1. **Sin argumento** (`/loop-mode`): lee `.claude/.loop-mode` y di al usuario el modo activo (o que no
   está fijado). No escribas nada.
2. **Con argumento** (`auto` | `human` | `oneshot`): valida que sea uno de los tres. Escribe ESE valor
   (una línea, sin más) en `$CLAUDE_PROJECT_DIR/.claude/.loop-mode`. Confirma el cambio y resume en una
   frase qué implica.
3. **Valor inválido**: recházalo y lista los tres válidos.

```bash
printf '%s\n' "<modo>" > "$CLAUDE_PROJECT_DIR/.claude/.loop-mode"
```

Tras fijar el modo, **adopta ese comportamiento de inmediato** para el resto de la sesión (ver
`loop-modes` para los gates de cada uno). El cambio persiste hasta que se vuelva a invocar `/loop-mode`.
