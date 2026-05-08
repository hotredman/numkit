# stats.regress/ridge — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** f92087f
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly on probed
inputs.

## Recommended fixes

1. **Spec extension** — fingerprint over weighted/unweighted,
   different rank cases. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: 15 fingerprints bit-identical numkit ↔ MATLAB ↔ Octave
  on scaled (default) and unscaled paths, single ridge parameter
  k and vector k. No code change needed.

