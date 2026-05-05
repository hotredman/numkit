# stats.dist/betacdf — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with norm/chi2/t/gam/exp `cdf`)
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/beta.cpp` (`betacdf`)
- Spec: `tools/parity/specs/betacdf.json`
- `'upper'` flag silently ignored — same shape as `normcdf`.

## MATLAB R2025b — actual behavior

- `p = betacdf(x, a, b)`
- `p = betacdf(x, a, b, 'upper')` — direct upper-tail.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `'upper'` flag silently returns lower-tail. Probe: `betacdf(0.5, 2, 3, 'upper')` MATLAB=`0.3125` vs numkit=`0.6875` (lower-tail) | **high** |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `betacdf(0.5, 2, 3)` | `0.6875` | identical ✅ |
| `betacdf(0.5, 2, 3, 'upper')` | `0.3125` | `0.6875` ❌ |

## Recommended fixes

Joint with `audit/findings/stats/normcdf.md` — same parser shape.
Compute `betainc(x, a, b, 'upper')` directly when flag set.

Spec: extend with `'upper'` fingerprint. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A — joint fix.
