# stats.dist/norminv — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/normal.cpp:101` (`norminv`)
- Adapter: `libs/stats/src/distributions/normal.cpp:174` (`norminv_reg`)
- Spec: `tools/parity/specs/norminv.json`
- `x = norminv(p[, mu, sigma])` — matches MATLAB exactly

## MATLAB R2025b — actual behavior

- `x = norminv(p[, mu, sigma])`
- (No `'upper'` flag in 2025b for `norminv`.)

## Gaps (numkit vs MATLAB)

**No major gap detected.**

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `norminv(0.5)` | `0` | `0` ✅ |
| `norminv(0.975)` | `1.9599639845` | identical ✅ |
| `norminv(0)` | `-Inf` | `-inf` ✅ |
| `norminv(1)` | `Inf` | `inf` ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint with `(mu, sigma)` ≠ (0, 1)
   and edge probabilities. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: ТЗ said "no major gap" but spec extension surfaced an
  out-of-range mismatch:
    numkit returned -Inf for p<0 and +Inf for p>1
    MATLAB / Octave both return NaN for p outside [0, 1]
  Fixed: norminv now distinguishes p == 0 / p == 1 (boundary,
  ±Inf) from p outside [0, 1] (NaN). NaN-input also propagates.
  18-fingerprint spec covering default + non-default mu/sigma +
  p=0/p=1 boundaries + p<0/p>1 invalid + sigma<=0 invalid.
  6 TEST_F gtest + smoke .m. Parity OK numkit ↔ MATLAB ↔ Octave.
