# stats.dist/gampdf — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/gamma_dist.cpp` (`gampdf`)
- Spec: `tools/parity/specs/gampdf.json`
- `Y = gampdf(X, a, b)` — matches MATLAB on valid params.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `gampdf(1, 0, 1)` invalid `a=0` | returns `0` | returns `nan` | medium (different edge convention) |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `gampdf(1.5, 2, 1)` | `0.3346952402` | identical ✅ |
| `gampdf([0.5 1 2]', 2, 1)` | `[0.303 0.368 0.271]` | identical ✅ |
| `gampdf(1, 0, 1)` | `0` | `nan` ❌ |

## Recommended fixes

1. **Match MATLAB's edge convention:** when `a==0` (or `b==0`),
   return `0` instead of `NaN`. Note this differs from `betapdf`
   which DOES return NaN on invalid params — different MATLAB
   conventions per family.
2. **Spec extension** — add fingerprint with edge cases.
   `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: Fixed a=0 edge: numkit returned NaN, MATLAB returns 0
  (degenerate at all x>0). Now: a<0 → NaN, a=0 → 0, b<=0 → NaN.
  Density at 0 already correct (Inf for a<1, 1/b for a=1, 0 for
  a>1). 12 fingerprints; 5 TEST_F gtest + smoke. Parity OK
  numkit ↔ MATLAB.
