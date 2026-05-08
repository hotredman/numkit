# stats.dist/binoinv — ТЗ for completion

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
- Notes: Pure spec coverage (impl already matched). 13-fingerprint
  spec covers scalar / vector q / boundaries (q∈{0,1}, p∈{0,1}) /
  invalid (q out of [0,1] / p out of [0,1] / n<0 / non-integer n).
  5 TEST_F gtest + smoke. Parity OK numkit ↔ MATLAB ↔ Octave.
