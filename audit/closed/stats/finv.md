# stats.dist/finv — ТЗ for completion

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
- Notes: Pure spec coverage (impl already matched). New
  tools/parity/specs/finv.json with 16 fingerprints covering
  F(1,1) heavy-tail / F(5,10) / F(10,30) × p ∈ {0.05, 0.5, 0.95}
  + boundaries (p=0/p=1) + p out-of-range + invalid v1/v2 ≤ 0.
  6 TEST_F gtest + smoke .m. Parity OK numkit ↔ MATLAB ↔ Octave.
