# stats/normfit — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:58` (`normfit`)
- Adapter: `libs/stats/src/fit/fit.cpp:476` (`normfit_reg`)
- Spec: `tools/parity/specs/normfit.json`
- What works today:
  - `[mu, sd, muci, sdci] = normfit(x[, alpha])`
  - Closed-form MLE; t-CI for `mu`, χ²-CI for `sigma`
  - Default `alpha = 0.05`
  - Edge: `N==0` ⇒ all `NaN`; `N==1` ⇒ `mu=x(1)`, `sd=0`, both CIs `NaN`

## MATLAB R2025b — actual behavior

Documented signatures (`help normfit`):

- `[muHat, sigmaHat] = normfit(x)`
- `[muHat, sigmaHat, muCI, sigmaCI] = normfit(x)`
- `[___] = normfit(x, alpha)`
- `[___] = normfit(x, alpha, censoring)`
- `[___] = normfit(x, alpha, censoring, freq)`
- `[___] = normfit(x, alpha, censoring, freq, options)` — `options` is
  a `statset('normfit')` struct controlling the iterative MLE used
  when censoring is active.

`muCI`, `sigmaCI` are 2×1 column vectors.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `normfit(x, alpha, censoring)` | iterative MLE on right-censored data; CI from observed Fisher info | not supported (extra args ignored) | high |
| 2 | `normfit(x, alpha, [], freq)` | weights observations | not supported | high |
| 3 | `normfit(x, alpha, censoring, freq)` | combined | not supported | high |
| 4 | `options` arg | controls Newton iteration | not supported | low |

## Reference table (from probe)

Inputs:
```
x    = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]'
cens = [0 0 0 0 0 1 1]'
freq = [2 2 2 1 1 1 1]'
alpha = 0.05
```

| Inputs | MATLAB |
|---|---|
| `[mu, sd, muci, sdci] = normfit(x)` | `mu=4.2142857143  sd=2.1050958580  muci=[2.2673967602; 6.1611746684]  sdci=[1.3565098940; 4.6355602532]` |
| `normfit(x, 0.05, cens)` | `mu=4.6418203244  sd=2.5994452841  muci=[2.6101081063; 6.6735325426]  sdci=[1.3275144566; 5.0900506217]` |
| `normfit(x, 0.05, [], freq)` | `mu=3.6200000000  sd=2.0186904446  muci=[2.1759158494; 5.0640841506]  sdci=[1.3885263593; 3.6853418310]` |
| `normfit(x, 0.05, cens, freq)` | `mu=3.8482097044  sd=2.3323500304  muci=[2.3655855351; 5.3308338737]  sdci=[1.3845490290; 3.9289736588]` |

numkit basic `[mu, sd, muci, sdci] = normfit(x)` matches the basic row.
The other rows return identical-to-basic outputs (extra args ignored),
which is silent-divergence — high severity because users will get plausible
numbers without warning.

## Recommended fixes

1. **Implement the censored-MLE branch.** When `cens` is supplied, the
   estimator becomes the maximiser of the censored normal log-likelihood
   — closed-form is no longer available. Use Newton iteration starting
   from the observed-events mean/sd; convergence criteria can be the
   defaults from `statset('normfit')` (TolFun=1e-8, MaxIter=200).
   The `muCI`/`sigmaCI` come from the inverse observed Fisher info —
   identical to the matrix `aVar` produced by `normlike` (see
   `audit/findings/stats/normlike.md`).
2. **Implement `freq` weighting.** When `cens` is empty but `freq` is
   supplied, the weighted closed-form MLE is `mu = sum(freq.*x)/sum(freq)`
   and `sd = sqrt( sum(freq.*(x-mu).^2) / (sum(freq)-1) )`. CIs use
   `dof = sum(freq) - 1`.
3. **Combined `cens + freq`:** fold weights into the iterative MLE.
4. **`options`:** accept the argument; when censoring is inactive it can
   be silently ignored. When censoring is active, honour at minimum
   `MaxIter` and `TolFun`.
5. **Spec extension:** add three new expressions and capture the eight
   numbers above. Fingerprint:
   `[mu_b, sd_b, muci_b(1), muci_b(2), sdci_b(1), sdci_b(2),`
   ` mu_c, sd_c, muci_c(1), muci_c(2), sdci_c(1), sdci_c(2),`
   ` mu_f, sd_f, muci_f(1), muci_f(2), sdci_f(1), sdci_f(2),`
   ` mu_cf, sd_cf, muci_cf(1), muci_cf(2), sdci_cf(1), sdci_cf(2)]`.
   `tol = 1e-7` (Newton iteration vs MATLAB's may differ at ~1e-9).
6. **PROGRESS.md row update:** new comment text — adds censoring + freq
   coverage, notes that `options` is best-effort.

## Out of scope for this ТЗ

- The `paramci` API and `NormalDistribution` object form — MATLAB
  channels the same calculation through several wrappers; numkit only
  needs the flat function form.
