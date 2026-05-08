# stats.dist/unidinv — ТЗ for completion

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
- Notes: Auditor "no major gap" wrong. Real bug found:
  `unidinv(0, N)` returned 1, MATLAB returns NaN (p=0 has no
  integer pre-image in {1..N}). Fixed: p<=0 or p>1 -> NaN; also
  tightened N guard to `!(N >= 1.0)` to catch NaN N. Octave uses
  a floor convention (gives 0/N-1 instead of ceil); we follow
  MATLAB. 15 fingerprints; 5 TEST_F gtest + smoke. Parity OK
  numkit ↔ MATLAB at tol=0.
