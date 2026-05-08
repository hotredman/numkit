# stats.dist/unidpdf — ТЗ for completion

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
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. 13 fingerprints
  (in-support + vector across edge + 3 out-of-support k + 4 bad N
  + NaN k/N). Parity OK numkit ↔ MATLAB at tol=0. Octave returns
  NaN for NaN k while MATLAB returns 0; we follow MATLAB. 4 TEST_F
  gtest + smoke.
