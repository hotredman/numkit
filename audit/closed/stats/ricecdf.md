# stats.dist/ricecdf — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** d68c22b
**Audit date:** 2026-05-06

## Notes

MATLAB does **not** ship direct names `nakapdf` / `ricepdf` etc.
— those distributions are accessed via the generic `pdf('Nakagami',
...)` / `pdf('Rician', ...)`. Direct names are an Octave +
numkit convention. Parity reference is therefore Octave's
statistics package.

## Gaps

**No major gap detected** vs Octave's direct-name implementation.
Numbers match.

## Recommended fixes

1. **Spec extension** — fingerprint over parameter sweeps. Cross-
   check against Octave's stats package output. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

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
