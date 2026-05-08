# stats.dist/tinv — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/students_t.cpp` (`tinv`)
- Spec: `tools/parity/specs/tinv.json`
- `x = tinv(p, nu)` — matches MATLAB exactly.

## MATLAB R2025b — actual behavior

`x = tinv(p, nu)`.

## Gaps (numkit vs MATLAB)

**No major gap detected.**

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `tinv(0.975, 5)` | `2.5705818356` | identical ✅ |
| `tinv(0.5, 10)` | `0` | `0` ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint with `nu = Inf` (Gaussian
   limit), edge p (0, 1), small-nu (1, 2). `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Auditor "no major gap" wrong on three counts. Real bugs
  fixed:
  1. `nu = Inf` returned NaN; now Gaussian limit (`norminv(p)`).
  2. `p < 0` returned `-Inf`; now `NaN`.
  3. `p > 1` returned `+Inf`; now `NaN`.
  Spec extended to 14 fingerprints (median + tails + small ν + Inf +
  4 boundary cases). Parity OK numkit ↔ MATLAB ↔ Octave at tol=1e-9
  (Gaussian-limit Newton-2 ~6e-13 from MATLAB reference). 6 TEST_F
  gtest + smoke.
