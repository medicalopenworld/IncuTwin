---
description: Prepara y publica una release desde dev a main con tag semver y back-merge (gitflow), para motherBoard o Display_HMI. Usar cuando hay cambios terminados en dev listos para producción.
argument-hint: "<motherboard|hmi> <major|minor|patch | X.Y.Z>"
disable-model-invocation: true
allowed-tools: Bash(git *), Bash(pio *), Bash(grep *)
---

# /git-release — publicar release a main

> **Atribución**: ningún commit/tag del release lleva `Co-Authored-By: Claude` ni `Claude-Session`. Autor único: Pablo Sánchez Bergasa (GitHub: `pablo18393`).

No hay `package.json` en este repo (PlatformIO/Arduino puro, no Node). `motherBoard` y `Display_HMI` **versionan por separado**, cada una con su propio `FWversion`:

- `motherBoard`: `#define FWversion "..."` en `motherBoard/include/config/board.h`.
- `Display_HMI`: `#define FWversion "..."` en `Display_HMI/include/main.h`.

Versión actual (según la placa indicada en `$ARGUMENTS`):
```bash
grep -oP '(?<=FWversion\s")[^"]+' motherBoard/include/config/board.h 2>/dev/null
grep -oP '(?<=FWversion\s")[^"]+' Display_HMI/include/main.h 2>/dev/null
```

> **Si el grep no imprime nada**: el fichero pudo haberse movido/renombrado desde que se escribió este skill. PARA y confírmalo con el usuario antes de asumir una versión — no sigas con `0.0.0` ni ningún valor por defecto silencioso.

Argumento (placa + bump o versión exacta): **$ARGUMENTS**

## Pasos

1. Parte de `dev` actualizado y limpio. Calcula la nueva versión semver a partir de `$ARGUMENTS` (major/minor/patch o `X.Y.Z` explícito) y de la versión actual leída arriba, para la placa indicada.
2. Crea la rama de release:
   ```bash
   git checkout dev
   git checkout -b release/<placa>-<ver>
   ```
3. Estabiliza (solo fixes, nada de features nuevas):
   - Actualiza el valor de `FWversion` en el header correspondiente a `<ver>`.
   - Si existe un `CHANGELOG.md` para esa placa o en `Firmware/`, mueve `## [Unreleased]` a `## [<ver>] - <fecha actual>`.
   - Corre `pio run -e main` (y `pio test -e native` si es `motherBoard` y el release toca `modules/control/`). Debe compilar/pasar sin errores — gate automatizable; la prueba en hardware real es manual.
   - Commit `chore(release): <placa> v<ver>`.
4. Integra en `main` y etiqueta:
   ```bash
   git checkout main
   git merge --no-ff release/<placa>-<ver> -m "release: <placa> v<ver>"
   git tag -a <placa>-v<ver> -m "release <placa> v<ver>"
   ```
5. Back-merge a `dev`:
   ```bash
   git checkout dev
   git merge --no-ff release/<placa>-<ver> -m "merge: release/<placa>-<ver> -> dev"
   git branch -d release/<placa>-<ver>
   ```
6. Reporta la versión publicada, el tag y el grafo (`git log --oneline --graph -20`).

Nota: el push a `main`/`dev` lo hace el usuario manualmente (los hooks bloquean push directo desde la sesión). Recuérdaselo con los comandos exactos (`git push origin main --tags`, `git push origin dev`).
