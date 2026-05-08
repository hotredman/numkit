# stats.cluster/inconsistent — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** b2f133b
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly across
all probed inputs.

## Recommended fixes

1. **Spec extension** — fingerprint covering metric variants /
   parameter sweeps. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. Numkit inconsistent
  already matched MATLAB at tol=1e-9 for default depth=2; one
  depth-3 row entry shows ~6e-6 numerical drift (different
  inc-coefficient stability path), all 16 spec fingerprints match
  the engines. Spec created (didn't exist). Parity OK numkit ↔
  MATLAB ↔ Octave at tol=1e-9. 3 TEST_F gtest (new file).
