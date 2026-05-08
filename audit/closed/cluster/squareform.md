# stats.cluster/squareform — ТЗ for completion

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
- Notes: Pure spec coverage, no impl change. Numkit squareform
  already matched MATLAB exactly across vector→matrix,
  matrix→vector, explicit 'tomatrix'/'tovector' modes, and
  scalar input.

  Spec created (didn't exist before) with 16 fingerprints (3-pt,
  4-pt, explicit modes, scalar). Parity OK numkit ↔ MATLAB ↔
  Octave at tol=0. 7 TEST_F gtest (new file) including
  round-trip identity check.
