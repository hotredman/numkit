# stats/jbtest — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:478` (`jbtest`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1362` (`jbtest_reg`)
- Spec: **none**
- What works today:
  - `[h, p, JB, cv] = jbtest(x[, alpha])`
  - `JB = n/6 · (S² + (K-3)²/4)`, computed using **biased** sample
    skewness/kurtosis (population formula)
  - `p = 1 - chi2cdf(JB, 2)`, `cv = chi2inv(1-α, 2)` — both
    asymptotic
  - Throws when `n < 4`

## MATLAB R2025b — actual behavior

Documented signatures (`help jbtest`):

- `h = jbtest(x)` / `h = jbtest(x, alpha)` / `h = jbtest(x, alpha, mctol)`
- `[h, p] = jbtest(___)` / `[h, p, jbstat, critval] = jbtest(___)`

`mctol` — Monte Carlo standard-error tolerance for refining the
small-sample table. When omitted, MATLAB uses a precomputed table of
critical values (Bera-Jarque small-sample distribution) and reports
the tabulated `p`/`critval`; for `p` outside the table (small JB),
MATLAB issues a warning *"P is greater than the largest tabulated
value, returning 0.5"* and returns `p = 0.5`. With `mctol`, MATLAB
runs Monte Carlo to refine.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | small-sample `p` / `critval` | precomputed table | asymptotic χ²(2) — significantly different at moderate n; e.g. n=10: MATLAB returns p=0.5 (table cutoff), critval=2.5239; numkit returns p=0.7172, cv=5.9915 | high |
| 2 | `mctol` argument | Monte Carlo refinement | not supported | medium |
| 3 | warning on out-of-table | issues warning | silently asymptotic | low |

## Reference table (from probe)

Inputs:
```
xn = [-0.5 0.3 0.7 1.1 -0.2 0.1 -0.4 0.8 -0.1 0.5]'    % n=10
JB = 0.66481 (matches both)
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[h,p,JB,cv] = jbtest(xn)` | `h=0, p=0.5, JB=0.6648, cv=2.5239` (warning) | `h=0, p=0.7172, JB=0.6648, cv=5.9915` |
| `jbtest(xn, 0.01)` | `p=0.5, cv=5.7077` | `p=0.7172, cv=9.2103` |
| `jbtest(xn, 0.05, 0.01)` (mctol) | `p=0.5273, cv=2.4622` | adapter rejects 3rd arg (`parse_alpha` would ignore — needs probe) |

## Recommended fixes

1. **Embed the small-sample critical-value table** — the same one
   MATLAB ships (sample sizes ~4..2000, alphas 0.001..0.5). The table
   is in the public domain (Bera & Jarque 1980 plus Urzúa 1996
   extension). Implementation:
   - For `n` in table range: bilinear interpolate `cv` from the table
     and report the table-lookup `p`.
   - Out-of-range upper: report the warning and clamp `p = 0.5`.
   - `n` larger than the largest table row: switch to χ²(2)
     asymptotic.
2. **Add optional `mctol` argument** — when present, run Monte Carlo
   simulation under H₀ (Gaussian samples of the same size), compute
   the empirical `p`, refine until `MC_se < mctol`. Use a fixed RNG
   seed so the spec output is reproducible.
3. **Spec creation:** `tools/parity/specs/jbtest.json` — fingerprint
   over basic / alpha=0.01 / large-n (asymptotic), plus a
   reproducible-MC entry under fixed seed. `tol = 1e-7` for table
   path; `tol = 1e-3` for MC path.
4. **PROGRESS.md row update:** drop "Jarque-Bera, JB ~ χ²(2)"; note
   table + MC + warning behavior.

## Out of scope for this ТЗ

- Multivariate JB (not in the standard MATLAB API).
