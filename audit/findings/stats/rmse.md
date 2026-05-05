# stats/rmse — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** ba142e6
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/descriptive/descriptive_extras.cpp:212`
  (`rmse`)
- Adapter: `libs/stats/src/descriptive/descriptive_extras.cpp:677`
  (`rmse_reg`)
- Spec: `tools/parity/specs/rmse.json`
- What works today:
  - `E = rmse(F, A[, dim])` — scalar dim

## MATLAB R2025b — actual behavior

Documented signatures (`help rmse`):

- `E = rmse(F, A)` / `(F, A, "all")` / `(F, A, dim)` / `(F, A, vecdim)`
- `(___, nanflag)` — `'omitnan'` (default) / `'includenan'`
- `(___, Weights=W)` (or `'Weights', W`)

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `rmse(F, A, "all")` | full-flatten | adapter `args[2].toScalar()` ⇒ throws | high |
| 2 | `rmse(F, A, [1 2])` (vecdim) | reduce | throws | high |
| 3 | `rmse(F, A, 'omitnan')` | drop NaN | throws | high |
| 4 | `rmse(F, A, 'Weights', W)` | weighted RMSE | throws | medium |
| 5 | default NaN behaviour | omitnan | NaN propagates | medium |

## Reference table (from probe)

Inputs:
```
F = [1 2 3]', A = [1.1 2.2 2.9]'
fc = [1 NaN 3 4 5]', ac = [2 4 6 8 10]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `rmse([1 2 3]', [1.1 2.2 2.9]')` | `0.1414213562` | identical ✅ |
| `rmse(A_mat, A_mat+0.1)` | `[0.1 0.1 0.1]` | identical ✅ |
| `rmse(A, A+0.1, "all")` | `0.1` | THROWS |
| `rmse(fc, ac, 'omitnan')` | `3.5707142143` | THROWS |
| `rmse(F, A, 'Weights', [1 2 3]')` | `0.1414213562` | THROWS |

## Recommended fixes

1. **Adapter rewrite (same dispatch shape as var/std/median):**
   detect string (`"all"` / `'omitnan'` / `'Weights'`) and vector
   `args[2]`, route accordingly. Then start a N-V loop for any
   remaining args.
2. **Implement `Weights` N-V:** weighted RMSE
   `√(Σ w_i (F_i - A_i)² / Σ w_i)`.
3. **Default omitnan for floating-point:** match MATLAB R2023b+
   (silent default change, document).
4. **Spec extension:** add fingerprint for "all", vecdim, omitnan,
   Weights. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.
