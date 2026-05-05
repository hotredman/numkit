# stats.dist/normpdf — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/normal.cpp:~50` (`normpdf`)
- Adapter: `libs/stats/src/distributions/normal.cpp:156` (`normpdf_reg`)
- Spec: `tools/parity/specs/normpdf.json` (if present)
- `Y = normpdf(X[, mu, sigma])` — defaults mu=0, sigma=1; matches
  MATLAB exactly

## MATLAB R2025b — actual behavior

`y = normpdf(x[, mu, sigma])`. `sigma <= 0` ⇒ NaN.

## Gaps (numkit vs MATLAB)

**No major gap detected.**

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `normpdf(0)` | `0.3989422804` | identical ✅ |
| `normpdf(1)` | `0.2419707245` | identical ✅ |
| `normpdf(2.5, 2, 0.5)` | `0.4839414490` | identical ✅ |
| `normpdf(1, 0, -1)` | `NaN` | `nan` ✅ |
| `normpdf(1, 0, 0)` | `NaN` | `nan` ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint with vector inputs and
   non-default `(mu, sigma)`. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.
