# stats/expfit — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:102` (`expfit`)
- Adapter: `libs/stats/src/fit/fit.cpp:502` (`expfit_reg`)
- Spec: `tools/parity/specs/expfit.json`
- What works today:
  - `[muhat, muci] = expfit(x[, alpha])`
  - `mu = mean(x)`; exact CI via `2N·muhat ~ mu·χ²(2N)`
  - Edge: `N==0` ⇒ `NaN` everywhere

## MATLAB R2025b — actual behavior

Documented signatures (`help expfit`):

- `muhat = expfit(data)`
- `[muhat, muci] = expfit(data)`
- `[muhat, muci] = expfit(data, alpha)`
- `[...] = expfit(data, alpha, censoring)`
- `[...] = expfit(data, alpha, censoring, freq)`

`muci` is a 2×1 column vector.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `expfit(x, alpha, cens)` | MLE = sum(x)/sum(1-cens); CI from chi² on observed events | not supported | high |
| 2 | `expfit(x, alpha, [], freq)` | MLE = sum(freq.*x)/sum(freq); CI uses `2*sum(freq)·muhat ~ mu·χ²(2·sum(freq))` | not supported | high |
| 3 | `expfit(x, alpha, cens, freq)` | combined: `mu = sum(freq.*x)/sum(freq.*(1-cens))`; CI uses observed-events `dof=2*sum(freq.*(1-cens))` | not supported | high |

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
| `[mu, ci] = expfit(x)` | `mu=5.5000000000  ci=[3.2192351616; 11.4693518055]` |
| `expfit(x, 0.05, cens)` | `mu=7.8571428571  ci=[4.2115019261; 19.5426101726]` |
| `expfit(x, 0.05, [], freq)` | `mu=4.6923076923  ci=[2.9100852755; 8.8125424263]` |
| `expfit(x, 0.05, cens, freq)` | `mu=6.1000000000  ci=[3.5704244520; 12.7205538206]` |

numkit basic matches; other three silently ignore extras.

## Recommended fixes

1. **Implement censored point estimate**:
   `muhat = sum(x) / sum(1 - cens)`. (Sum of all observation times
   over count of failures.) CI uses chi² with
   `dof = 2·sum(1 - cens)`: `lo = 2·sum(x)/χ²(1-α/2, dof)`,
   `hi = 2·sum(x)/χ²(α/2, dof)`.
2. **Implement freq-only branch**:
   `muhat = sum(freq.*x)/sum(freq)`, CI from
   `dof = 2·sum(freq)`.
3. **Combined**:
   `muhat = sum(freq.*x) / sum(freq.*(1-cens))`,
   `dof = 2·sum(freq.*(1-cens))`.
4. **Spec extension:** new fingerprint
   `[mu_b, ci_b(1), ci_b(2),`
   ` mu_c, ci_c(1), ci_c(2),`
   ` mu_f, ci_f(1), ci_f(2),`
   ` mu_cf, ci_cf(1), ci_cf(2)]`. `tol = 1e-9`.
5. **PROGRESS.md row update:** new comment text covering cens + freq.

## Out of scope for this ТЗ

- `expfit` does not document an `options` argument.
- The `distributed/expfit` overload — out of scope.
