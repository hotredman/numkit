# stats.dist/ncx2cdf — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with cdf family)
**Audited at commit:** d68c22b
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | `'upper'` flag silently ignored. Probe: `ncx2cdf(2, 3, 1, 'upper')` MATLAB=0.6917 vs numkit=0.3082 (lower-tail). | **high** |

## Recommended fixes

Joint with `audit/findings/stats/normcdf.md` parser fix. After
this batch, the cdf-'upper' gap covers **21 cdf functions**.

## Out of scope for this ТЗ

- N/A — joint fix.
