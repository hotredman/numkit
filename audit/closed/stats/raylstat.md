# stats.dist/raylstat — ТЗ for completion

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
- Notes: Vectorisation via emit_vec_stat_1arg (sweep 5dd32c38).
  Impl edges already correct (b<=0 → NaN). 10-fingerprint spec;
  3 TEST_F gtest + smoke. Parity OK numkit ↔ MATLAB ↔ Octave.
