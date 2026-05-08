# stats.dist/binopdf — ТЗ for completion

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
- Notes: Pure spec coverage. 14-fingerprint covers scalar/vector,
  out-of-support (k<0/k>n/non-integer → 0), boundary (p=0 only k=0,
  p=1 only k=n → 1), invalid (n<0/p out of [0,1] → NaN). 5 TEST_F
  gtest + smoke. Parity OK numkit ↔ MATLAB ↔ Octave at tol=1e-12.
