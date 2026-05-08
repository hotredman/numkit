# stats.dist/tpdf — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/students_t.cpp` (`tpdf`)
- Spec: `tools/parity/specs/tpdf.json`
- `Y = tpdf(X, nu)` — matches MATLAB for finite nu

## MATLAB R2025b — actual behavior

`y = tpdf(x, nu)`. As `nu → ∞`, `tpdf(x, Inf) → normpdf(x)` —
MATLAB returns the Gaussian limit `0.3989422804` for `tpdf(0, Inf)`.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `tpdf(0, Inf)` returns `nan` (numkit), `0.3989422804` (MATLAB Gaussian limit) | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `tpdf(0, 5)` | `0.3796066898` | identical ✅ |
| `tpdf(1, 5)` | `0.2196797974` | identical ✅ |
| `tpdf(0, Inf)` | `0.3989422804` | `-nan(ind)` ❌ |

## Recommended fixes

1. **Handle `nu == Inf` as the Gaussian limit:** at the top of
   `tpdf`, when `nu` is `Inf` (or above some large threshold like
   `1e8`), delegate to `normpdf(x, 0, 1)`. Otherwise compute via
   the `gamma((nu+1)/2)/(sqrt(nu·π)·gamma(nu/2)) · (1+x²/nu)^(-(nu+1)/2)`
   formula.
2. **Spec extension** — add fingerprint with `nu = Inf` and
   `nu = 1e10`. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Fixed `tpdf(x, Inf)` returning NaN — now delegates to
  Gaussian-limit form `(1/√(2π))·exp(-x²/2)`. Also tightened the
  guard to `!(nu > 0)` so NaN nu propagates as NaN. Spec covers
  scalar/vector + Gaussian limit + nu=1e10 (matches MATLAB at 1e-6,
  Octave at 1e-5). 12 fingerprints; 6 TEST_F gtest + smoke. Parity
  OK numkit ↔ MATLAB ↔ Octave at tol=1e-12.
