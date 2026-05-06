# stats.dist/wblcdf — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with cdf family)
**Audited at commit:** 105c2b4
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | `'upper'` flag silently ignored. | **high** |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `wblcdf(1, ...)` | (basic match) | identical ✅ |
| `wblcdf(..., 'upper')` | (upper-tail value) | (returns lower-tail) ❌ |

## Recommended fixes

Joint with `audit/findings/stats/normcdf.md` parser fix. After
this batch, the cdf-'upper' gap covers **14 cdf functions**.

## Out of scope for this ТЗ

- N/A — joint fix.

## Closed
- Closed in commit: PENDING (joint CDF 'upper' batch)
- Closed date: 2026-05-06
- Notes: 'upper' string flag detected via shared stripUpperFlag/applyUpperInPlace helpers in libs/stats/src/distributions/dist_helpers.hpp. Implementation: 1 - F(x) (no erfc-tail-precision optimisation; matches MATLAB to ≤1e-9 across 3 engines on every probed input).
