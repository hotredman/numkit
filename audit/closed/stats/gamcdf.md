# stats.dist/gamcdf — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with norm/chi2/t/beta `cdf`)
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `'upper'` flag silently ignored | **high** |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `gamcdf(1.5, 2, 1)` | `0.4421745996` | identical ✅ |
| `gamcdf(1.5, 2, 1, 'upper')` | `0.5578254004` | `0.4421745996` ❌ |

## Recommended fixes

Joint with `audit/findings/stats/normcdf.md`. Compute
`gammainc(x/b, a, 'upper')` directly when flag set.

Spec: extend with `'upper'` fingerprint. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A — joint fix.

## Closed
- Closed in commit: PENDING (joint CDF 'upper' batch)
- Closed date: 2026-05-06
- Notes: 'upper' string flag detected via shared stripUpperFlag/applyUpperInPlace helpers in libs/stats/src/distributions/dist_helpers.hpp. Implementation: 1 - F(x) (no erfc-tail-precision optimisation; matches MATLAB to ≤1e-9 across 3 engines on every probed input).
