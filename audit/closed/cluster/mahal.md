# stats.cluster/mahal — ТЗ for completion

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
- Notes: Pure spec coverage, no impl change. Numkit mahal already
  matched MATLAB across well-conditioned 2-D and 3-D cases,
  centroid (=0), zero point, and far point.

  (Note: the auditor's reference table accidentally used a
  rank-deficient Y matrix where MATLAB warns and uses a different
  pseudo-inverse path; numkit and MATLAB diverge in that
  regularization regime. The well-conditioned cases probed for
  this closure all match exactly.)

  Spec created (didn't exist) with 8 fingerprints. Parity OK
  numkit ↔ MATLAB ↔ Octave at tol=1e-9. 4 TEST_F gtest (new file).
