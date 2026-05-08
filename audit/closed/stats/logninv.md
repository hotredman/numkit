# stats.dist/logninv — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 105c2b4
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly across
all probed inputs.

## Recommended fixes

1. **Spec extension** — fingerprint over parameter sweeps + edge
   cases. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: Tightened edge handling: previous `pi <= 0 → 0` returned
  0 even for q<0 (silently). Now: q<0 / q>1 / NaN → NaN; q=0 → 0;
  q=1 → Inf. 11 fingerprints; 5 TEST_F gtest + smoke. Parity OK
  numkit ↔ MATLAB ↔ Octave at tol=1e-12.
