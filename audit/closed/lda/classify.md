# stats.lda/classify — ТЗ for completion

**Status:** closed (mahalanobis type deferred)
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

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: All 4 documented discriminant types match MATLAB R2025b
  exactly: 'linear' (default), 'quadratic', 'diaglinear',
  'diagquadratic'. Verified on a 3-class 2-D dataset with empirical
  priors and Cholesky-factor solver. Classifications, error rate,
  log-prob, and posterior probabilities all bit-identical across
  24 fingerprints.

  Mahalanobis discriminant type DEFERRED — already throws cleanly.

  4 artefacts shipped (impl unchanged + 24-fp parity spec extension
  + 5 gtests + smoke). Octave doesn't ship `classify`.

