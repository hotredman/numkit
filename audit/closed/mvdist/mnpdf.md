# stats.mvdist/mnpdf — ТЗ for completion

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
- Notes: Auditor's "no major gap" verified — numkit's mnpdf was
  already bit-identical to MATLAB R2025b / Octave on row-vector
  and matrix inputs. Spec extended with 6 fingerprints; verified
  at tol=1e-12. No code change.

