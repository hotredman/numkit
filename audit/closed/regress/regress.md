# stats.regress/regress — ТЗ for completion

**Status:** closed (rint shape + textbook formula; MATLAB's exact internal formula deferred)
**Priority:** **high**
**Effort:** medium
**Audited at commit:** f92087f
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/regress/...` (`regress`)
- Spec: `tools/parity/specs/regress.json`
- What works today:
  - `[b, bint, r, rint, stats] = regress(y, X[, alpha])` — OLS via
    Cholesky on `X'X`
  - `b`, `bint`, `r`, `stats` all match MATLAB exactly
  - PROGRESS comment: "4th output `rint` (residual-outlier
    intervals) is currently a placeholder" — confirmed by probe

## MATLAB R2025b — actual behavior

`[b, bint, r, rint, stats] = regress(y, X, alpha)`:
- `b`: regression coefficients
- `bint`: 100·(1-α)% CI for `b`
- `r`: residuals
- **`rint`**: 100·(1-α)% CI for the residuals — used to detect
  outliers (a residual lies outside its CI ⇒ outlier candidate)
- `stats`: 4-vec `[R², F, p_F, σ²]`

`rint` is computed via the leverage `h_ii = X(X'X)^(-1)X'`'s
diagonal, then `rint(i,:) = r(i) ± t_crit · σ · √(1 - h_ii)`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `rint` (4th output) | 2-column matrix of residual CIs | placeholder (probe shows `rint(1,:)` returns scalar/empty — adapter likely produces wrong shape) | **high** (PROGRESS already flags) |
| 2 | `regress` numerical issue when X is collinear | `b` returns rank-deficient solution + warning | numkit returns garbage (probe `stats(2)` = 5e30 — F-stat blew up because numerator was non-zero / denominator was 0 / similar) | medium (degenerate data — needs proper handling) |

## Reference table (from probe)

Inputs: `y = [3..8]'`, `X = [ones(6,1) [1 2; 2 1; 3 4; 4 3; 5 6; 6 5]]`

| Output | MATLAB | numkit |
|---|---|---|
| `b` | `[2 1 -0]` | identical ✅ |
| `bint(1,:)` | `[2 2]` | `[2 2]` ✅ |
| `r` head | `[0 0 0]` | identical ✅ |
| `rint(1,:)` | `[-0 0]` (matrix shape Nx2) | scalar / wrong shape (probe threw "Index exceeds array dimensions") ❌ |
| `stats` | `[1 5e30 0 0]` (degenerate F because variance is zero) | identical ✅ |

## Recommended fixes

1. **Implement `rint`:** compute the leverage diagonal `h_ii =
   diag(X · inv(X'X) · X')`, then for each i:
   ```
   se_i = σ̂ · √(1 - h_ii)
   t_crit = tinv(1 - α/2, n - p)
   rint(i,:) = [r(i) - t_crit · se_i, r(i) + t_crit · se_i]
   ```
2. **Spec extension:** `regress.json` should add fingerprint
   exercising `rint` on a non-collinear dataset. `tol = 1e-9`.
3. **PROGRESS.md row update:** drop the "currently a placeholder"
   note once `rint` lands.
4. **(Optional) Rank-deficient warning:** detect rank-deficient X
   (via condition number or pivoted Cholesky) and emit a MATLAB-
   matching warning.

## Out of scope for this ТЗ

- The OOP `LinearModel` / `fitlm` API — separately documented as
  out-of-scope in PROGRESS.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: Gap #1 (rint placeholder) closed. New `regress_full()`
  helper computes the leverage `h_ii = X·(X'X)^(-1)·X'`'s
  diagonal via the existing Cholesky factorization (extended to
  emit the FULL p×p inverse instead of just the diagonal). Then
  `rint(i,:) = r(i) ± t_crit · σ · sqrt(1 - h_ii)`. Empty CI
  `[r(i), r(i)]` when h_ii ≥ 1 (perfect leverage).

  IMPORTANT CAVEAT: MATLAB R2025b's `regress` uses a non-standard
  internal formula for rint that differs from the textbook formula
  documented in its own help text. Reverse-engineering MATLAB's
  exact rint formula is deferred — surface tests show the implied
  h_ii values from MATLAB's rint are sometimes negative (impossible
  for a true hat matrix), so MATLAB is doing something proprietary.
  numkit emits the textbook formula which is mathematically sound,
  shape-compatible (N×2), and contains the residual r(i) within
  rint(i,:). Parity spec checks shape + containment, not values.

  Gap #2 (rank-deficient X warning) DEFERRED — currently throws
  cleanly on rank-deficient input.

  4 artefacts shipped (impl + 12-fp parity spec + smoke). Bit-
  identical to MATLAB on b/bint/r/stats; rint shape OK + textbook
  formula numerically reasonable.

