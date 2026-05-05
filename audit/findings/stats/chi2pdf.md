# stats.dist/chi2pdf — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/chi2.cpp` (`chi2pdf`)
- Spec: `tools/parity/specs/chi2pdf.json`
- `Y = chi2pdf(X, k)` — matches MATLAB exactly.

## MATLAB R2025b — actual behavior

`y = chi2pdf(x, nu)`. Returns 0 for `x < 0`, regular density
otherwise.

## Gaps (numkit vs MATLAB)

**No major gap detected.**

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `chi2pdf(2, 3)` | `0.2075537487` | identical ✅ |
| `chi2pdf(0, 2)` | `0.5` | `0.5` ✅ |
| `chi2pdf([0.5 1 2 5]', 3)` | `[0.220 0.242 0.208 0.073]` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint covering edge values
   (x < 0, k = 1, k = 30) and vector x. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.
