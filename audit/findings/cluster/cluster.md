# stats.cluster/cluster — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** b2f133b
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/cluster/...` (`cluster`)
- Spec: `tools/parity/specs/cluster.json`
- `T = cluster(Z, 'maxclust', K)` — matches MATLAB exactly.
- `T = cluster(Z, 'cutoff', V)` — uses **distance** as criterion;
  MATLAB default is **inconsistency**.

## MATLAB R2025b — actual behavior

- `T = cluster(Z, 'cutoff', c)` — uses INCONSISTENCY criterion by
  default (MATLAB's documented default)
- `T = cluster(Z, 'cutoff', c, 'criterion', 'distance')` — uses
  distance threshold
- `T = cluster(Z, 'maxclust', K)` — produces exactly K clusters
- `T = cluster(Z, 'cutoff', c, 'depth', d)` — depth for
  inconsistency calc

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | default `'cutoff'` criterion | `'inconsistency'` | numkit silently uses `'distance'`. Probe: `cluster(Z, 'cutoff', 5)` MATLAB returns `[1 1 1 1 1 1 1 1]` (1 cluster, all consistent), numkit returns `[1 1 1 2 2 2 3 3]` (3 clusters by distance) | medium (silent default divergence) |
| 2 | `'criterion'` N-V | `distance` / `inconsistency` selection | not supported | medium |
| 3 | `'depth'` N-V | configurable inconsistency depth | not supported | low |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `cluster(Z, 'maxclust', 3)` | `[1 1 1 2 2 2 3 3]` | identical ✅ |
| `cluster(Z, 'cutoff', 5)` | `[1 1 1 1 1 1 1 1]` (inconsistency) | `[1 1 1 2 2 2 3 3]` (distance) ❌ |
| `cluster(Z, 'cutoff', 5, 'criterion', 'distance')` | `[1 1 1 2 2 2 3 3]` | (criterion N-V not parsed) |

## Recommended fixes

1. **Switch default criterion to `'inconsistency'`** to match
   MATLAB. Implement the inconsistency calc (already available
   via `inconsistent()` function which numkit has).
2. **Add `'criterion'` N-V** with values `distance` /
   `inconsistency` (MATLAB also accepts `MaxClust`).
3. **Add `'depth'` N-V** for inconsistency depth (default 2).
4. **Spec extension** — add fingerprint for both criteria and
   for non-default depth.

## Out of scope for this ТЗ

- N/A.
