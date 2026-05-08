# stats.dist/raylinv — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** e580a5c
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly.

## Recommended fixes

1. **Spec extension** — fingerprint over parameter sweeps + edge
   cases. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage. 10 fingerprints (median, vector q,
  q=0/q=1 boundaries, q<0/q>1, b<=0); parity OK numkit ↔ MATLAB ↔
  Octave at tol=1e-12. 4 TEST_F gtest + smoke.
