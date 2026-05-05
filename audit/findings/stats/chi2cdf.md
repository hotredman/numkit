# stats.dist/chi2cdf — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with `normcdf`/`tcdf`)
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/chi2.cpp` (`chi2cdf`)
- Spec: `tools/parity/specs/chi2cdf.json`
- `p = chi2cdf(X, k)` — matches MATLAB; `'upper'` flag silently
  ignored.

## MATLAB R2025b — actual behavior

- `p = chi2cdf(x, nu)`
- `p = chi2cdf(x, nu, 'upper')` — direct upper-tail, more accurate
  in the right tail than `1 - chi2cdf(x, nu)`.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `'upper'` flag silently ignored — returns lower-tail value | **high** |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `chi2cdf(2, 3)` | `0.4275932955` | identical ✅ |
| `chi2cdf(2, 3, 'upper')` | `0.5724067045` | `0.4275932955` ❌ |

## Recommended fixes

1. **Add `'upper'` flag parsing** — same shape as the
   `audit/findings/stats/normcdf.md` fix. When `upper`, compute
   `gammainc(x/2, k/2, 'upper')` directly rather than `1 - lower`.
2. **Spec extension** — add fingerprint with `'upper'` and a deep
   right-tail value (e.g., `chi2cdf(50, 3, 'upper')` ≈ 7.74e-11)
   to verify the precision.

## Out of scope for this ТЗ

- N/A — joint fix with normcdf/tcdf.
