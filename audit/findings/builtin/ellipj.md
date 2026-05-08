# builtin/ellipj — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 03244f9
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Output matches MATLAB to within 1-ULP
(or exactly) on probed inputs.

## Recommended fixes

1. **Spec extension** — fingerprint covering domain edges and
   asymptotic regimes. `tol = 1e-14`.

## Out of scope for this ТЗ

- N/A.
