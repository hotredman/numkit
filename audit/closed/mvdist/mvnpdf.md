# stats.mvdist/mvnpdf — ТЗ for completion

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
- Notes: Auditor's "no major gap" claim verified — numkit's mvnpdf
  was already bit-identical to MATLAB R2025b across default
  (mu=0, Σ=I), explicit mu, and explicit Σ paths. Spec extended
  with 13 fingerprints across all three signature variants plus
  scalar / 1-D inputs. Bit-identical numkit ↔ MATLAB ↔ Octave
  at tol=1e-12. No code change needed.

