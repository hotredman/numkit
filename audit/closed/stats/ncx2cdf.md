# stats.dist/ncx2cdf — ТЗ for completion

**Status:** closed
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

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: 'upper' string flag now stripped via shared
  numkit::stats::detail::stripUpperFlag and applied via
  applyUpperInPlace (see libs/stats/src/distributions/dist_helpers.hpp).
  Implementation: 1 - F(x) — no erfc-tail-precision optimisation; matches
  MATLAB R2025b (Octave fallback when MATLAB doesn't ship the function)
  to specified tol on every probed input. Closed jointly with 8 sibling
  cdf functions in one cycle (the gap was identical across all of them).
