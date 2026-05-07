# stats.dist/chi2inv — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/chi2.cpp` (`chi2inv`)
- Spec: `tools/parity/specs/chi2inv.json`
- `x = chi2inv(p, k)` — matches MATLAB exactly.

## MATLAB R2025b — actual behavior

`x = chi2inv(p, nu)`. Vector inputs supported.

## Gaps (numkit vs MATLAB)

**No major gap detected.**

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `chi2inv(0.95, 3)` | `7.8147279033` | identical ✅ |
| `chi2inv(0.5, 10)` | `9.3418177656` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint with edge p (0, 1) and
   common df (1, 5, 30). `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: Same edge mismatch as chi2pdf — ТЗ said "no major gap"
  but `chi2inv(p, 0)` returned NaN; MATLAB returns 0 (degenerate
  distribution → all mass at 0). Fixed: `k < 0` → NaN, `k == 0` →
  0 for p∈[0,1] / NaN otherwise. Spec covers k∈{1,5,30} × p∈{0.05,
  0.5, 0.95} + p=0/p=1 boundaries + p out-of-range + k=0/k<0 edges.
  7 TEST_F gtest + smoke .m. Parity OK numkit ↔ MATLAB at tol=1e-9.
