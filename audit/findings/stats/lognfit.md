# stats/lognfit — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:144` (`lognfit`)
- Adapter: `libs/stats/src/fit/fit.cpp:528` (`lognfit_reg`)
- Spec: `tools/parity/specs/lognfit.json`
- What works today:
  - `[parm, pci] = lognfit(x[, alpha])`
  - Closed-form on `log(x)`: `mu = mean(log(x))`, `sd` is sample std
    (N-1) of `log(x)`
  - `pci` is a 2×2 matrix: column 1 = `mu` CI, column 2 = `sigma` CI
  - Edge: `N<2` ⇒ `parm` and `pci` all `NaN`; any `x<=0` ⇒ all `NaN`

## MATLAB R2025b — actual behavior

Documented signatures (`help lognfit`):

- `pHat = lognfit(x)`
- `[pHat, pCI] = lognfit(x)`
- `[pHat, pCI] = lognfit(x, alpha)`
- `[___] = lognfit(x, alpha, censoring)`
- `[___] = lognfit(x, alpha, censoring, freq)`
- `[___] = lognfit(x, alpha, censoring, freq, options)`

`pHat` is `1×2`, `pCI` is `2×2` (rows: lower/upper; cols: mu/sigma).

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `lognfit(x, alpha, cens)` | iterative MLE on right-censored | not supported | high |
| 2 | `lognfit(x, alpha, [], freq)` | freq-weighted closed-form on log(x) | not supported | high |
| 3 | `lognfit(x, alpha, cens, freq)` | combined | not supported | high |
| 4 | `options` arg | controls Newton iteration | not supported | low |

## Reference table (from probe)

Inputs:
```
x    = [1 2 3 4 5 6 7 8 9 10]'
cens = [0 0 0 0 0 0 0 1 1 1]'
freq = [2 2 2 1 1 1 1 1 1 1]'
alpha = 0.05
```

| Inputs | MATLAB |
|---|---|
| `[ph, pc] = lognfit(x)` | `parm=[1.5104412573 0.7330238657]  pci=[0.9860675727 0.5041996222; 2.0348149419 1.3382158333]` |
| `lognfit(x, 0.05, cens)` | `parm=[1.6856246465 0.9277676177]  pci=[1.0724981385 0.5288251852; 2.2987511544 1.6276697414]` |
| `lognfit(x, 0.05, [], freq)` | `parm=[1.2997055417 0.7840916903]  pci=[0.8258836754 0.5622611625; 1.7735274080 1.2943277053]` |
| `lognfit(x, 0.05, cens, freq)` | `parm=[1.4214576479 0.9373415041]  pci=[0.8924682721 0.5887466083; 1.9504470238 1.4923382706]` |

numkit basic call matches; other three return basic-call values
(silent ignore of extra args).

## Recommended fixes

1. **Implement censored MLE on `log(x)`** — equivalent to running
   `normfit` with cens on the transformed data; CI is on
   `[mu sigma]` of the underlying normal.
2. **Implement `freq` weighting** — closed-form weighted moments on
   `log(x)`; CI uses `dof = sum(freq) - 1`.
3. **Combined `cens + freq`** — fold into iterative MLE.
4. **`options`** — accept-and-honour at minimum `MaxIter`/`TolFun`.
5. **Spec extension:** add the three new expressions; fingerprint
   should cover all four `parm`/`pci` quartets — 24 entries total
   (4 cases × (2 parm + 4 pci)). `tol = 1e-7`.
6. **PROGRESS.md row update:** drop trailing wording, add full coverage
   note.

## Out of scope for this ТЗ

- The `LognormalDistribution` object form and `paramci` wrapper —
  numkit only needs the flat function.
