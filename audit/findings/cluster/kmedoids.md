# stats.cluster/kmedoids — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** b2f133b
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/cluster/...` (`kmedoids`)
- Spec: `tools/parity/specs/kmedoids.json`
- PAM-style implementation. Cluster STRUCTURE matches MATLAB
  but the cluster ID labels differ (depends on the random init
  picking different starting medoids).

## MATLAB R2025b — actual behavior

- `[idx, C] = kmedoids(X, K)` — k-medoids clustering
- `[idx, C, sumd] = kmedoids(...)`
- `[idx, C, sumd, D, midx, info] = kmedoids(...)`
- N-V: `'Algorithm'` (`'pam'` / `'small'` / `'large'`),
  `'Distance'`, `'Replicates'`, `'Start'`

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | rng-seeded init | deterministic medoid choice from `rng(seed)` | numkit uses different RNG → different starting medoids → different cluster ID labels (structure same) | low (RNG cascade from `unifrnd`) |
| 2 | 6-output form `[idx, C, sumd, D, midx, info]` | full diagnostics | needs probe — likely 2-output only | medium |
| 3 | `'Algorithm'`, `'Replicates'`, `'Start'` N-V | configurable | needs probe | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `rng(42); kmedoids(X, 3)` idx | `[1 1 1 3 3 3 2 2]` | `[2 2 3 1 1 1 1 1]` (different labels — same partition?) |

The numkit partition splits into clusters but the partitioning
itself isn't bit-identical to MATLAB's (cluster {1,2,3} groups
points differently). Either:
1. Different RNG init produces different local optima (different
   medoid choices yield different final partitions when ties
   exist).
2. Different `Replicates` default — MATLAB defaults to 1 but
   docs note it's bumped for larger inputs.

## Recommended fixes

1. **Joint with `audit/findings/stats/normrnd.md`** — once the
   base RNG matches MATLAB, kmedoids deterministic with `rng(seed)`
   should produce MATLAB-compat labels.
2. **Add 4-output and 6-output forms** for the documented
   diagnostics.
3. **Add `'Algorithm'`, `'Replicates'`, `'Start'` N-V** parsing.
4. **Spec extension** — once RNG matches, add fingerprint for
   reproducible-seed labels.

## Out of scope for this ТЗ

- N/A.
