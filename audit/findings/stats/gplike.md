# stats/gplike — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:440` (`gplike`)
- Adapter: `libs/stats/src/fit/fit.cpp:641` (`gplike_reg`)
- Spec: `tools/parity/specs/gplike.json`
- What works today:
  - `nL = gplike([k sigma], data)` — two-parameter GP, `theta=0`
  - Branches for `k=0` and `k≠0`
  - Returns `+inf` for `sigma<=0`, any `data(i)<0`, or per-point
    support violation `1 + k*x/sigma <= 0`

## MATLAB R2025b — actual behavior

Documented signatures (`help gplike`):

- `nlogL = gplike(params, data)`
- `[nlogL, acov] = gplike(params, data)` — 2×2 inverse-observed-Fisher

`gplike` does **not** document `cens`/`freq`.

Edge convention from probe:
- `sigma <= 0` ⇒ presumably `NaN` (not directly probed but consistent
  with the other `*like` family).
- **`data(i) < 0` is NOT auto-rejected** — MATLAB evaluates the formula
  as long as `1 + k*x/sigma > 0` per point. Probe with
  `gplike([0.5, 1], [-1 1 2]')` returned a finite `1.2163953243`
  (every per-point `t = 1 + 0.5*x` is `0.5, 1.5, 2.0`, all positive).

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `[nL, acov] = gplike(params, data)` | 2×2 matrix | not produced | medium |
| 2 | `gplike(params, x)` with `x(i)<0` but `1+k*x(i)/sigma > 0` | finite nL | returns `+inf` | high |

## Reference table (from probe)

Inputs:
```
data = [1 2 3 4 5]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `nL = gplike([0.5, 1], data)` | `13.0988348331` | `13.0988348331` ✅ |
| `acov(1,1)` k>0 | `0.5357555979` | (not produced) |
| `acov(1,2)` = `acov(2,1)` k>0 | `-0.3457416827` | (not produced) |
| `acov(2,2)` k>0 | `0.3689249582` | (not produced) |
| `nL = gplike([0, 1], data)` | `15.0000000000` | `15.0000000000` ✅ |
| `acov(1,1)` k=0 | `0.0322580645` | (not produced) |
| `acov(2,2)` k=0 | `0.1225806452` | (not produced) |
| `gplike([0.5, 1], [-1 1 2]')` | `1.2163953243` | `inf` ❌ |

## Recommended fixes

1. **Add second output `acov`** — 2×2 inverse-observed-Fisher matrix
   (parameter order `[k sigma]`). Numerical Hessian acceptable; for
   `k=0` evaluate analytically (the GP reduces to exponential with
   rate `1/sigma`).
2. **Drop the `xi < 0` early-exit:** the per-point support is
   `1 + k*x/sigma > 0`, not `x >= 0`. With negative `k` (bounded
   support) the same check still applies. With `k = 0`, any finite
   `x` is fine — but the canonical GP support assumes `x >= 0` for
   `k >= 0`; MATLAB's behaviour above suggests it does not enforce
   the lower-bound `x >= theta` either, only the per-point `t > 0`.
   The probe value `1.2163953243` is the contract; reproduce it.
3. **Spec extension:** fingerprint
   `[nL_kpos, nL_k0,`
   ` ac_kpos(1,1), ac_kpos(1,2), ac_kpos(2,2),`
   ` ac_k0(1,1), ac_k0(2,2),`
   ` nL_xneg]`. `tol = 1e-9` for `nL`; `1e-7` if `acov` is from finite
   differences.
4. **PROGRESS.md row update:** drop *"Default-path only"* clause.

## Out of scope for this ТЗ

- The `theta` (location/threshold) parameter — MATLAB's `gplike` is
  documented as taking `[k sigma]` only with implicit `theta=0`.
- The `gpfit` companion (currently ❌ in PROGRESS) — separate work.
