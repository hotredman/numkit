# stats.dist/expcdf — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with the cdf family)
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `'upper'` flag silently ignored | **high** |
| 2 | `expcdf(x)` 1-arg form likely throws (similar to `exppdf`) | medium |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `expcdf(1, 2)` | `0.3934693403` | identical ✅ |
| `expcdf(1, 2, 'upper')` | `0.6065306597` | likely returns lower-tail ❌ |

## Recommended fixes

Joint with `audit/findings/stats/normcdf.md` (the cdf-`'upper'`
parser). Plus default `mu = 1` per `exppdf` ТЗ.

Spec: extend with `'upper'` and 1-arg form. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.
