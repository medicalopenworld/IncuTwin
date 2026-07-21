---
description: Integra la rama de feature actual en dev con merge --no-ff (historial legible). Usar al terminar el loop de una feature de motherBoard/Display_HMI/shared, con el build en verde.
disable-model-invocation: true
allowed-tools: Bash(git *), Bash(pio *)
---

# /git-feature-finish — integrar feature en dev

Rama actual: !`git branch --show-current`
Commits de la feature (sobre dev): !`git log --oneline dev..HEAD 2>/dev/null | head -20`

## Pasos

1. Verifica que estás en una rama `feat/*`. Si no, PARA.
2. Comprueba que el build compila antes de integrar, en la(s) placa(s) que toca la feature:
   ```bash
   (cd motherBoard && pio run -e main)   # si el diff toca motherBoard/ o shared/
   (cd Display_HMI && pio run -e main)   # si el diff toca Display_HMI/ o shared/
   (cd motherBoard && pio test -e native)   # si toca modules/control/{alarm_machine,pid_wrapper}.cpp
   ```
   Si falla, PARA y reporta. Esto es un gate de compilación/test nativo, no de hardware real: la
   prueba en el dispositivo físico es manual — recuerda al usuario que la haga él mismo si el cambio
   la requiere antes de considerar la feature realmente verificada.
3. Asegúrate de que no hay cambios sin commitear (`git status --short`).
4. Integra en `dev` con merge sin fast-forward:
   ```bash
   slug=$(git branch --show-current)
   git checkout dev
   git merge --no-ff "$slug" -m "merge: $slug -> dev"
   ```
5. Borra la rama de feature ya integrada:
   ```bash
   git branch -d "$slug"
   ```
6. Reporta: el merge commit, el árbol resultante (`git log --oneline --graph -15`) y recuerda que las releases se publican con `/git-release`.

No hagas push a `main`/`dev` (lo bloquea un hook); la publicación es responsabilidad de `/git-release`.
