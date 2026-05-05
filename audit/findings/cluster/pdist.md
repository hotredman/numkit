# stats.cluster/pdist — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** b2f133b
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/cluster/...` (`pdist`)
- Spec: `tools/parity/specs/pdist.json`
- All probed standard metrics (euclidean, cityblock, minkowski,
  cosine) match MATLAB exactly.

## MATLAB R2025b — actual behavior

- `D = pdist(X)` — euclidean default
- `D = pdist(X, metric)` — built-in name OR a custom function
  handle `@(u, V) ...`
- `D = pdist(X, 'minkowski', p)` — extra parametric arg
- `D = pdist(X, 'mahalanobis', C)` — covariance matrix arg

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `pdist(X, fn_handle)` | applies `fn(u, V)` per pair | adapter throws "Cannot convert function_handle to scalar" | medium |
| 2 | `pdist(X, 'mahalanobis', C)` | uses supplied covariance | needs probe — likely uses internal cov | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `pdist(X)` (eucl) | (28-vec match) | identical ✅ |
| `pdist(X, 'cityblock')` | (28-vec match) | identical ✅ |
| `pdist(X, 'minkowski', 3)` | head `[1 1 5.04 5.74 5.74]` | identical ✅ |
| `pdist(X, 'cosine')` | head `[0.05 0.05 0 0 0]` | identical ✅ |
| `pdist(X, @(u,V)sum(abs(u-V),2))` | head `[1 1 8 9 9]` | THROWS |

## Recommended fixes

1. **Accept function-handle metric** — when `args[1]` is a
   `function_handle`, call it on each `(u, V)` pair (where `u` is
   row i and `V` is rows i+1..end).
2. **Implement `'mahalanobis', C`** — accept the covariance matrix
   as the 3rd positional arg (in addition to metric).
3. **Spec extension** — add fingerprint for custom-fn metric and
   `mahalanobis` with explicit C.

## Out of scope for this ТЗ

- N/A.
