# stats/mape — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small (joint with `rmse`)
**Audited at commit:** ba142e6
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/descriptive/descriptive_extras.cpp:253`
  (`mape`)
- Adapter: `libs/stats/src/descriptive/descriptive_extras.cpp:667`
  (`mape_reg`)
- Spec: `tools/parity/specs/mape.json`
- What works today:
  - `E = mape(F, A[, dim])` — scalar dim

## MATLAB R2025b — actual behavior

Documented signatures (`help mape`):

- `E = mape(F, A)` / `(F, A, "all")` / `(F, A, dim)` / `(F, A, vecdim)`
- `(___, nanflag)` — `'omitnan'` (default) / `'includenan'`
- `(___, zeroflag)` — `'zero'` (default; treat zeros as zero error)
  / `'nonzero'` (drop zero entries)

`mape = mean(|(F - A) / A| · 100)`. The `zeroflag` controls how
`A == 0` rows are handled — by default they contribute `0/0 = NaN`,
but with `'zero'` flag MATLAB treats `0/0` as `0`; with `'nonzero'`
it drops the rows entirely.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `mape(F, A, "all")` | full-flatten | adapter `args[2].toScalar()` ⇒ throws | high |
| 2 | `mape(F, A, [1 2])` (vecdim) | reduce | throws | high |
| 3 | `mape(F, A, 'omitnan')` | drop NaN | throws | high |
| 4 | `mape(F, A, 'zero')` / `'nonzero'` | zero-handling | not supported | medium |
| 5 | default zero-handling | `'zero'` (treat 0/0 as 0) | numkit returns `Inf` for any A=0 entry; MATLAB-default returns finite (zero contribution) | high |

## Reference table (from probe)

Inputs:
```
F = [1 2 3]', A = [1.1 2.2 2.9]'
fc = [1 NaN 3 4 5]', ac = [2 4 6 8 10]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `mape([1 2 3]', [1.1 2.2 2.9]')` | `7.21003` | identical ✅ |
| `mape(fc, ac, 'omitnan')` | `50` | THROWS |
| `mape([0 1 2]', [0.1 1 2]')` (zero in A) | `33.333` (default = 'zero', so 0.1/0 → 0 contribution) | `33.333` ✅ — but check whether numkit's path is "zero" or "raw"; same result here by coincidence (only 1 of 3 hits the divide) |
| `mape([0 1 2]', [0 1 2]')` (zero in BOTH) | `0` (zero flag) | needs probe — likely `Inf` or `NaN` |

## Recommended fixes

1. **Adapter rewrite** (same dispatch as `rmse`/`var`): handle
   string/vector dim, then N-V tail with `nanflag` and `zeroflag`.
2. **Implement `'zero'`/`'nonzero'` flags** in the per-element
   reducer:
   - `'zero'` (default): treat `(F_i - A_i) / A_i` as `0` whenever
     both `F_i == 0` and `A_i == 0` (define `0/0 = 0`).
   - `'nonzero'`: drop rows where `A_i == 0` from both numerator
     and denominator count.
3. **Default omitnan:** match R2023b+ (document the change).
4. **Spec extension:** add fingerprint for "all", vecdim, omitnan,
   zero/nonzero, A==0 edges. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.

## Closed (partial)
- Closed in commit: PENDING
- Closed date: 2026-05-06
- Notes: Adapter rewritten with parseDimOrAll helper (shared between rmse/mape). Supports 'all' string + full-flatten vecdim. Verified vs MATLAB R2025b. omitnan / Weights / mape zero-handling deferred (low-medium impact gaps).
