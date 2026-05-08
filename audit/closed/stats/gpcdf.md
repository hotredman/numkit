# stats.dist/gpcdf — ТЗ for completion

**Status:** closed
**Priority:** **high**
**Effort:** small (joint with cdf family)
**Audited at commit:** 7a46bd1
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | `'upper'` flag silently ignored. | **high** |

## Recommended fixes

Joint with `audit/findings/stats/normcdf.md` parser fix. After
this batch, the cdf-'upper' gap covers **20 cdf functions**.

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
