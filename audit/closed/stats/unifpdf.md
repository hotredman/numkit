# stats.dist/unifpdf — ТЗ for completion

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
- Notes: Auditor "no major gap" wrong on one count.
  `unifpdf(NaN, 0, 1)` returned 0 (NaN comparison falls through to
  the else branch); MATLAB returns NaN. Fixed: explicit `isnan(xi)`
  -> NaN check. 17 fingerprints (defaults + wider interval + vector
  + bad-params + NaN x). Parity OK numkit ↔ MATLAB at tol=1e-12.
  Octave's unifpdf doesn't ship the 1-arg default form; we follow
  MATLAB. 5 TEST_F gtest + smoke.
