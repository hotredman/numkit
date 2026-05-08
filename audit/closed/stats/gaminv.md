# stats.dist/gaminv — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.**

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `gaminv(0.5, 2, 1)` | `1.6783469900` | identical ✅ |

## Recommended fixes

1. **Spec extension** — fingerprint over (a, b) variations. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: Found same a=0 edge as gampdf (ТЗ said "no major gap").
  numkit returned NaN for gaminv(p, 0, b); MATLAB returns 0
  (degenerate quantile = 0 for any p∈[0,1]). Fixed: a<0 → NaN,
  a=0 → 0 for p∈[0,1] / NaN otherwise, b<=0 → NaN. 11 fingerprints;
  6 TEST_F gtest + smoke. Parity OK numkit ↔ MATLAB.
