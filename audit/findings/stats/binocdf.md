# stats.dist/binocdf — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with cdf family)
**Audited at commit:** 1525319
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | `'upper'` flag silently ignored. Probe: `binocdf(3, 10, 0.4, 'upper')` MATLAB=`0.7350` vs numkit=`0.2650` (lower-tail) | **high** |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `binocdf(3, 10, 0.4)` | `0.2650259153` | identical ✅ |
| `binocdf(3, 10, 0.4, 'upper')` | `0.7349740847` | `0.2650259153` ❌ |

## Recommended fixes

Joint with `audit/findings/stats/normcdf.md` parser fix.

## Out of scope for this ТЗ

- N/A.
