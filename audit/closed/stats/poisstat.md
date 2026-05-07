# stats.dist/poisstat — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 1525319
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly across
all probed inputs (including `*rnd` under `rng(42)` — discrete
RNG appears to match MATLAB bit-for-bit, unlike the continuous
RNG family).

## Recommended fixes

1. **Spec extension** — fingerprint covering parameter sweeps +
   edge cases. `tol = 0` (integer-stable for discrete).

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: Two fixes:
  - Vectorisation via emit_vec_stat_1arg (sweep commit 5dd32c38).
  - poisstat(0) returned (0, 0); MATLAB returns NaN/NaN (degenerate).
    Fixed: lambda <= 0 ⇒ NaN.
  3 TEST_F gtest + smoke .m. Parity OK numkit ↔ MATLAB ↔ Octave.
