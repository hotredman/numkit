# stats.lda/classify — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** f92087f
**Audit date:** 2026-05-06

## Gaps

**No major gap detected** on the documented signature surface.
Both numkit and MATLAB throw on degenerate (collinear) training
data with identical positive-definite covariance error.

## Recommended fixes

1. **Spec extension** — fingerprint covering each `type`:
   `'linear'` (default), `'quadratic'`, `'diaglinear'`,
   `'diagquadratic'` on a non-degenerate dataset. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.
