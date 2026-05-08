# stats.dim/pcacov — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** f92087f
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly on probed
inputs.

## Recommended fixes

1. **Spec extension** — fingerprint over different X shapes.
   `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: Auditor's "no major gap" verified. 9 fingerprints
  bit-identical numkit ↔ MATLAB ↔ Octave at tol=1e-9. No code
  change.

