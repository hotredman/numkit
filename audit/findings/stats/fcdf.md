# stats.dist/fcdf — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with cdf family)
**Audited at commit:** e580a5c
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | `'upper'` flag silently ignored. Probe: `fcdf(2, 5, 10, 'upper')` MATLAB=`0.1642` vs numkit=`0.8358` (lower-tail) | **high** |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `fcdf(2, 5, 10)` | `0.8358050491` | identical ✅ |
| `fcdf(2, 5, 10, 'upper')` | `0.1641949509` | `0.8358050491` ❌ |

## Recommended fixes

Joint with `audit/findings/stats/normcdf.md` parser fix.

## Out of scope for this ТЗ

- N/A.
