# stats.dist/raylcdf — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with cdf family)
**Audited at commit:** e580a5c
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | `'upper'` flag silently ignored. Probe: `raylcdf(1, 1, 'upper')` MATLAB=`0.6065` vs numkit=`0.3935` (lower-tail) | **high** |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `raylcdf(1, 1)` | `0.3934693403` | identical ✅ |
| `raylcdf(1, 1, 'upper')` | `0.6065306597` | `0.3934693403` ❌ |

## Recommended fixes

Joint with `audit/findings/stats/normcdf.md`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING (joint CDF 'upper' batch)
- Closed date: 2026-05-06
- Notes: 'upper' string flag detected via shared stripUpperFlag/applyUpperInPlace helpers in libs/stats/src/distributions/dist_helpers.hpp. Implementation: 1 - F(x) (no erfc-tail-precision optimisation; matches MATLAB to ≤1e-9 across 3 engines on every probed input).
