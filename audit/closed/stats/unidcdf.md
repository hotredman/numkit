# stats.dist/unidcdf — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with cdf family)
**Audited at commit:** 1525319
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | `'upper'` flag silently ignored. Probe: `unidcdf(3, 5, 'upper')` MATLAB=`0.4` vs numkit=`0.6` (lower-tail) | **high** |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `unidcdf(3, 5)` | `0.6` | `0.6` ✅ |
| `unidcdf(3, 5, 'upper')` | `0.4` | `0.6` ❌ |

## Recommended fixes

Joint with `audit/findings/stats/normcdf.md` parser fix. After
this batch, the `'upper'` flag is missing on **12 cdf functions**
(norm, chi2, t, beta, gam, exp, f, unif, rayl, bino, poiss, unid).
A single shared parser update closes all 12.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING (joint CDF 'upper' batch)
- Closed date: 2026-05-06
- Notes: 'upper' string flag detected via shared stripUpperFlag/applyUpperInPlace helpers in libs/stats/src/distributions/dist_helpers.hpp. Implementation: 1 - F(x) (no erfc-tail-precision optimisation; matches MATLAB to ≤1e-9 across 3 engines on every probed input).
