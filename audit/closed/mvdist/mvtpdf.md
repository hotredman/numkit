# stats.mvdist/mvtpdf — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** f92087f
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly on probed
inputs.

## Recommended fixes

1. **Spec extension** — fingerprint over multivariate parameters.
   `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: Auditor's "no major gap" verified — numkit's mvtpdf was
  already bit-identical to MATLAB R2025b across multiple covariance
  / df values. Spec extended with 11 fingerprints. Octave differs
  from MATLAB when C is a scaling matrix rather than correlation
  (different convention) — out of numkit's scope. No code change.

