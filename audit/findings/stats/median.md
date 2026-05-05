# stats/median — ТЗ for completion

**Status:** open
**Priority:** high
**Effort:** medium
**Audited at commit:** ba142e6
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/descriptive/descriptive.cpp:245` (`median`)
- Adapter: `libs/stats/src/descriptive/descriptive.cpp:974` (`median_reg`)
- Spec: `tools/parity/specs/median.json`
- What works today:
  - `M = median(A[, dim])`
  - `M = median(A, 'all')` — char-typed flag (not string-typed)
  - `M = median(___, 'omitnan')` / `'includenan'`

## MATLAB R2025b — actual behavior

Documented signatures (`help median`):

- `M = median(A)` / `(A, 'all')` / `(A, dim)` / `(A, vecdim)`
- `(___, missingflag)` — `'omitnan'` default
- `(___, Weights=W)` — weighted median (R2023b+)

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `median(A, "all")` (string scalar literal) | full-flatten | adapter only checks `ValueType::CHAR`; `"all"` is `STRING` ⇒ falls through to `toScalar()` ⇒ throws | medium (string-vs-char trap) |
| 2 | `median(A, [1 2])` (vecdim) | reduce over dims | adapter calls `args[1].toScalar()` ⇒ throws | high |
| 3 | `median(A, Weights=W)` / `median(A, 'Weights', W)` | weighted median | adapter throws "median: unknown flag 'weights'" | medium |

## Reference table (from probe)

Inputs:
```
A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11]
v = [2 5 3 7 4 6 NaN 8 1 9]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `median([1 2 3 4 5]')` | `3` | `3` ✅ |
| `median(A)` | `[3 6 9]` | `[3 6 9]` ✅ |
| `median(v)` (default) | `NaN` (legacy) / `5.0` (R2023b+ omitnan default) | `5.5` (numkit's "default = omit") ❌ |
| `median(A, 'all')` (char) | `6` | needs probe — likely OK |
| `median(A, "all")` (string) | `6` | THROWS |
| `median(A, [1 2])` | `6` | THROWS |
| `median([1:5]', 'Weights', [1 2 3 2 1]')` | `3` | THROWS |
| `median(v, 'omitnan')` | `5` | `5` ✅ |

Note the default-mode divergence: numkit returns `5.5` for
`median(v)`, while MATLAB legacy returns `NaN` and MATLAB R2023b+
returns `5` (since R2023b default is `'omitnan'`). Three different
contracts; numkit matches none.

## Recommended fixes

1. **Treat `STRING`-typed `"all"` like `CHAR`-typed `'all'`.** In the
   adapter, replace the type check with `if (a.isChar() ||
   a.isString())`.
2. **Accept vector `dim`** (`isVector()` branch) — reduce
   sequentially across the named dims.
3. **Implement `Weights` N-V:** parse trailing
   `('Weights', W)` (and `Weights=W` if numkit's parser supports
   struct-style N-V — appears not to). Weighted median:
   - Sort `(x_i, w_i)` by `x_i`.
   - Compute cumulative weight; the median is the smallest `x_k`
     such that `cumW(k) ≥ ΣW / 2`. Average with `x_{k+1}` when
     `cumW(k) == ΣW / 2`.
4. **Default-mode decision:** match MATLAB R2023b+ default of
   `'omitnan'` for floating-point inputs, OR keep current "default
   includes NaN but returns 5.5" — both are wrong. Pick one and
   document. R2023b+ matching is the safer long-term choice.
5. **Spec extension:** add fingerprint for `"all"` (string), vecdim,
   weighted, default-NaN behaviour. `tol = 1e-9`.

## Out of scope for this ТЗ

- The `mediano f differences` form for paired data (n/a — that's
  a different test, e.g. `signrank`).
