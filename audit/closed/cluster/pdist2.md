# stats.cluster/pdist2 — ТЗ for completion

**Status:** closed (partial — function-handle deferred)
**Priority:** medium
**Effort:** small
**Audited at commit:** b2f133b
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/cluster/...` (`pdist2`)
- Spec: `tools/parity/specs/pdist2.json`
- Same metric set as `pdist`.

## MATLAB R2025b — actual behavior

- `D = pdist2(X, Y, metric)`
- `[D, I] = pdist2(X, Y, metric, 'Smallest', k)` — return k
  smallest distances per row, plus indices
- `[D, I] = pdist2(X, Y, metric, 'Largest', k)` — k largest
- `D = pdist2(X, Y, 'mahalanobis', C)` — covariance arg

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `pdist2(A, B, 'eucl', 'Smallest', k)` 2-output form | top-k nearest distances + indices | adapter throws "pdist: unknown metric 'Smallest'" — N-V parser collapses with metric-name parser | medium |
| 2 | `'Largest', k` mirror form | bottom-k farthest | not supported | medium |
| 3 | function-handle metric | `pdist2(A, B, fn)` | not supported | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `pdist2(A, B)` (eucl) row 1 | `[6.40 6.40 12.73 13.45]` | identical ✅ |
| `[D, I] = pdist2(A, B, 'eucl', 'Smallest', 2)` | D row 1 = `[1 1 7.07 7.81]`, I row 1 = `[4 4 4 4]` (matrix shape 2×4) | THROWS |

## Recommended fixes

1. **Add N-V parser for `'Smallest'`/`'Largest'`** — separate from
   the metric arg (which is positional). When set, return per-row
   top-k or bottom-k as a `k × Ny` matrix plus an index matrix as
   the 2nd output.
2. **Function-handle metric** — same fix as `pdist`.
3. **Spec extension** — add fingerprint for `Smallest` and
   `Largest` forms.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: Added 'Smallest'/'Largest' k N-V mode in `pdist2_reg`
  (per-column partial_sort, returns k×My distance + 1-based index
  matrix). Also fixed a latent bug: default Mahalanobis was using
  cov(Y) but MATLAB R2025b uses cov(X) (the FIRST arg) — verified by
  direct probe. Function-handle metric (gap #3) deferred — same
  status as `pdist`.

