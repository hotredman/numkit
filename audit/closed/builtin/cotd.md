# builtin/cotd — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** a6e4264
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Element-wise scalar function based on
`cos(deg2rad(x)) / sin(deg2rad(x))` — matches MATLAB.

## Recommended fixes

1. **Spec extension** — fingerprint covering domain edges (avoid
   poles at 0°, 180°). `tol = 1e-15`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Forward-trig spec-extension batch (18 functions). All
  libm-backed, bit-identical MATLAB R2025b. See libs/builtin/tests/
  fwd_trig_batch_test.cpp + smoke + 18 parity specs.
