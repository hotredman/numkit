# stats.dist/nakapdf — ТЗ for completion

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
- Notes: Multi-namespace batch (44 funcs across io+comm+stats).
  Bit-identical MATLAB R2025b on probed inputs.
