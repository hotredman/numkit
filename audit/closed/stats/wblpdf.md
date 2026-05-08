# stats.dist/wblpdf — ТЗ for completion

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
- Closed date: 2026-05-08
- Notes: Auditor "no major gap" mostly right but one MSVC quirk
  fixed: `wblpdf(NaN, 1, 1)` returned signaling -nan(ind) (broke
  parity parser); now explicit `isnan(xi)` -> quiet NaN. Also
  tightened param guard to `!(a>0)||!(b>0)` so NaN params
  propagate. 18 fingerprints (defaults + scaled + density-at-0 by
  shape (3 regimes) + vector + 4 bad-params + NaN x). Parity OK
  numkit ↔ MATLAB ↔ Octave at tol=1e-12. 6 TEST_F gtest + smoke.
