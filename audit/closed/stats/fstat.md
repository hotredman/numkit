# stats.dist/fstat — ТЗ for completion

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
- Closed date: 2026-05-07
- Notes: ТЗ said "no major gap" but spec extension surfaced two
  fixes (same pattern as betastat / chi2stat / expstat):
    1. Adapter was scalar-only — added MATLAB-style broadcasting
       (equal sizes OR one scalar; otherwise dim error).
    2. Impl skipped v1 ≤ 0 / v2 ≤ 0 validation, so `fstat(0, 10)`
       returned 1.25/Inf instead of NaN/NaN. Now both NaN.
  15-fingerprint spec covers scalar / vector / v2≤2 (mean NaN) /
  v2≤4 (variance NaN) / invalid (v1=0, v2=0, v1<0). 4 TEST_F
  gtest + smoke .m. Parity OK numkit ↔ MATLAB ↔ Octave.
