# stats.dist/tcdf — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with `normcdf`/`chi2cdf`)
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/students_t.cpp` (`tcdf`)
- Spec: `tools/parity/specs/tcdf.json`
- `p = tcdf(X, nu)` — matches MATLAB; `'upper'` flag silently
  ignored.

## MATLAB R2025b — actual behavior

- `p = tcdf(x, nu)`
- `p = tcdf(x, nu, 'upper')` — direct upper-tail.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `'upper'` flag silently ignored — returns lower-tail | **high** |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `tcdf(0, 5)` | `0.5` | `0.5` ✅ |
| `tcdf(2, 5)` | `0.9490302606` | identical ✅ |
| `tcdf(-2, 5)` | `0.0509697394` | identical ✅ |
| `tcdf(2, 5, 'upper')` | `0.0509697394` | `0.9490302606` ❌ |

## Recommended fixes

1. **Add `'upper'` flag parsing** — joint with normcdf/chi2cdf.
   When set, compute `1 − tcdf(x, nu)` via the symmetry-aware
   identity (avoids subtraction loss at large `|x|`).
2. **Spec extension** — add fingerprint with `'upper'` and a
   far-tail value (e.g., `tcdf(20, 5, 'upper')` ≈ 2.7e-6).

## Out of scope for this ТЗ

- N/A — joint fix.

## Closed
- Closed in commit: PENDING (joint CDF 'upper' batch)
- Closed date: 2026-05-06
- Notes: 'upper' string flag detected via shared stripUpperFlag/applyUpperInPlace helpers in libs/stats/src/distributions/dist_helpers.hpp. Implementation: 1 - F(x) (no erfc-tail-precision optimisation; matches MATLAB to ≤1e-9 across 3 engines on every probed input).
