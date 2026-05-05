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
