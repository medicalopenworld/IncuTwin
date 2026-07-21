---
name: git-flow
description: Convenciones de git, ramas y commits del framework Genesis (gitflow + Conventional Commits + merge --no-ff + releases semver) para motherBoard/Display_HMI/shared. Úsalo al crear ramas, hacer commits por stage del loop, integrar features o preparar releases.
---

# Gitflow en Genesis (capa motherBoard/Display_HMI/shared)

## Estándar de nombres (convención única)

Fuente de verdad de cómo se nombra TODO en esta capa. Respétalo siempre (lo refuerza el paso 7 de learnings).

| Artefacto                 | Patrón                                                | Ejemplo                                   |
| ------------------------- | ----------------------------------------------------- | ------------------------------------------------ |
| Rama de **producto**      | `feat/<slug-kebab>`                                   | `feat/alarm-debounce`                     |
| Rama de **capa agéntica** | `meta/<slug-kebab>`                                   | `meta/generalize-claude-framework`        |
| Rama de release           | `release/<x.y.z>`                                     | `release/17.4`                            |
| Rama de hotfix            | `hotfix/<slug-kebab>`                                 | `hotfix/uart-flood`                       |
| Commit                    | `tipo(scope): descripción` (Conventional, imperativo) | `fix(motherboard): debounce de alarma`    |
| Tag de release            | `v<x.y.z>` (semver, por placa: `mb-v17.4`/`hmi-v2.4.0` si se versionan por separado) | `mb-v17.4`             |
| Change de OpenSpec        | `mb-<slug>` / `hmi-<slug>` / `shared-<slug>`          | `mb-alarm-debounce`                       |
| ADR                       | `Firmware/docs/adr/<NNNN>-<slug>.md` (4 dígitos, serie propia de esta capa) | `Firmware/docs/adr/0001-...` |
| Retro                     | `Firmware/docs/retro/<YYYY-MM-DD>-<feature-slug>.md`  | `Firmware/docs/retro/2026-07-08-...md`    |

**Slug**: kebab-case, ASCII, corto y descriptivo del _qué_.

**Scopes de commit habituales**: `motherboard`, `hmi`, `shared`, `meta` (framework/.claude/momentum). No hay `commitlint` (no es un proyecto Node): el formato lo exige la convención y lo verifica `code-reviewer` en el stage de review; los `merge:` están exentos.

## Ramas

| Rama            | Propósito                                              | Recibe de                | Integra con                   |
| --------------- | ------------------------------------------------------ | ------------------------- | ------------------------------ |
| `main`          | Producción. Releases versionadas (tag semver `vX.Y.Z`) | `release/*`                | —                              |
| `dev`           | Integración continua                                   | `feat/*` (merge --no-ff)  | `release/*`                    |
| `feat/<slug>`   | Una feature = un loop                                  | `dev`                      | `dev` (merge --no-ff)          |
| `release/<ver>` | Estabiliza cambios terminados                          | `dev`                      | `main` (+ back-merge a `dev`)  |
| `hotfix/<slug>` | Arreglo urgente                                        | `main`                      | `main` y `dev`                 |

> Un hook bloquea el push directo a `main`/`dev`: se actualizan solo vía merge desde `feature-finish`/`release`.

## Commits atómicos por stage

Cada stage del loop produce **un commit** con el tipo Conventional correcto:

| Stage        | Tipo de commit                         |
| ------------ | --------------------------------------- |
| Explore      | `chore(spec): explore <feat>`           |
| Propose      | `docs(spec): propose <feat>`            |
| Design (ADR) | `docs(adr): <decisión>`                 |
| Red          | `test: failing specs for <feat>`        |
| Green        | `feat: implement <feat>`                |
| Refactor     | `refactor: <feat>`                      |
| Verify       | `test: verify <feat>`                   |
| Review       | `fix: review feedback`                  |
| Docs         | `docs: update for <feat>`               |
| Archive      | `chore(spec): archive <feat>`           |
| Retro        | `chore(meta): retro + improve .claude`  |

Atribución: el único autor es **Pablo Sánchez Bergasa** (GitHub: `pablo18393`); jamás `Co-Authored-By: Claude/Anthropic` ni `Claude-Session`.

Reglas: un commit hace una sola cosa coherente y toca una sola placa (salvo `shared/`, que por diseño afecta a ambas); el mensaje explica el porqué cuando no es obvio.

## Integración de una feature

```bash
git checkout dev && git pull
git checkout -b feat/<slug>
# ... loop con commits atómicos por stage ...
git checkout dev
git merge --no-ff feat/<slug> -m "merge: feat/<slug> -> dev"
git branch -d feat/<slug>
```

## Release

```bash
git checkout -b release/<ver> dev
# estabilizar (solo fixes), bump de version (ver git-release), CHANGELOG si existe
git checkout main && git merge --no-ff release/<ver>
git tag -a <ver> -m "release <ver>"
git checkout dev && git merge --no-ff release/<ver>   # back-merge
```

Versionado semver: MAJOR (breaking), MINOR (feature), PATCH (fix). `motherBoard` y `Display_HMI` versionan cada una su propio `FWversion` (ver skill `git-release`) — pueden liberarse de forma independiente.
