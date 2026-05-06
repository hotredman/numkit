# stats/normlike — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:281` (`normlike`)
- Adapter: `libs/stats/src/fit/fit.cpp:581` (`normlike_reg`)
- Spec: `tools/parity/specs/normlike.json`
- What works today:
  - `nL = normlike([mu sigma], data)` — basic negative log-likelihood
  - `nL = normlike([mu sigma], data, cens)` — right-censored term `-log(S(z))`
  - `nL = normlike([mu sigma], data, [], freq)` — frequency weights
  - `nL = normlike([mu sigma], data, cens, freq)` — both
  - Edge: empty data ⇒ 0; sigma ≤ 0 ⇒ NaN; NaN in data ⇒ NaN; zero-freq rows dropped

## MATLAB R2025b — actual behavior

Documented signatures (`help normlike`):

- `nlogL = normlike(params, x)`
- `nlogL = normlike(params, x, censoring)`
- `nlogL = normlike(params, x, censoring, freq)`
- `[nlogL, aVar] = normlike(___)` — second output is the **inverse of the
  observed Fisher information matrix** evaluated at `params`. For two
  parameters the result is a 2×2 symmetric matrix with order `[mu sigma]`.

`aVar` exists for every signature including censoring + freq.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `[nlogL, aVar] = normlike(params, x)` | returns 2×2 inverse-observed-Fisher matrix | adapter has `nargout` declared but only emits the first output | medium |
| 2 | `[nlogL, aVar] = normlike(params, x, cens[, freq])` | same; aVar reflects cens/freq weighting | same — second output never produced | medium |

## Reference table (from probe)

Inputs (shared across rows):
```
x    = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]'
cens = [0 0 0 0 0 1 1]'
freq = [2 2 2 1 1 1 1]'
params = [3, 1.5]
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `nL = normlike(params, x)` | `17.4730477114` | `17.4730477114` ✅ |
| `aVar(1,1)` basic | `0.5685760656` | (not produced) |
| `aVar(1,2)` = `aVar(2,1)` basic | `-0.1526499228` | (not produced) |
| `aVar(2,2)` basic | `0.0942837759` | (not produced) |
| `nL = normlike(params, x, cens)` | `18.6858148256` | `18.6858148256` ✅ |
| `aVar(1,1)` cens | `0.5719686586` | (not produced) |
| `aVar(1,2)` cens | `-0.1426189548` | (not produced) |
| `aVar(2,2)` cens | `0.0841378833` | (not produced) |
| `nL = normlike(params, x, [], freq)` | `22.2484808576` | `22.2484808576` ✅ |
| `aVar(1,1)` freq | `0.2663412361` | (not produced) |
| `aVar(1,2)` freq | `-0.0500095598` | (not produced) |
| `aVar(2,2)` freq | `0.0604954352` | (not produced) |
| `nL = normlike(params, x, cens, freq)` | `23.4612479718` | `23.4612479718` ✅ |
| `aVar(1,1)` both | `0.2704780402` | (not produced) |
| `aVar(1,2)` both | `-0.0476691396` | (not produced) |
| `aVar(2,2)` both | `0.0551473719` | (not produced) |

## Recommended fixes

1. **Add second output `aVar`:** must produce the 2×2 inverse of the
   observed Fisher information matrix evaluated at the input
   `[mu, sigma]`, in column-major storage with the parameter order
   `[mu, sigma]`. Censoring and frequency weighting must be reflected.
   Do not derive from the formula in MATLAB internals — the values in
   the Reference table above are the contract; reproduce them within
   `tol = 1e-9`.
2. **Spec extension:** new `expr` line capturing the four `aVar` paths
   plus all four `nL` paths. Add fingerprint entries:
   `[nL_basic, av_basic(1,1), av_basic(1,2), av_basic(2,2),`
   ` nL_cens,  av_cens(1,1),  av_cens(1,2),  av_cens(2,2),`
   ` nL_freq,  av_freq(1,1),  av_freq(1,2),  av_freq(2,2),`
   ` nL_both,  av_both(1,1),  av_both(1,2),  av_both(2,2)]`.
   `tol = 1e-9` is sufficient.
3. **PROGRESS.md row update:** drop the trailing
   *"Second output `avar` (inverse observed-Fisher info) NOT
   implemented"* clause from the comment; replace with
   *"Default + censoring + freq + `aVar` (inverse observed-Fisher 2×2)."*
4. **Smoke test (optional):** existing smoke covers nL only; extend to
   exercise `[nL, aVar] = normlike(...)` and assert symmetry of aVar.

## Out of scope for this ТЗ

- Validation of `freq` containing fractional or negative weights — this
  ТЗ is about adding the second output, not redefining input handling.
- Vector-of-`params` broadcasting (MATLAB `params` is documented as a
  fixed 2-element vector).

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-06
- Notes: `normlike_reg` honours `nargout >= 2` and emits the 2×2
  inverse observed-Fisher matrix in column-major order (parameter
  order [mu, sigma]). Analytical Hessian implemented for both
  uncensored (`I_μμ = w/σ²`, etc.) and right-censored
  (using hazard `h = φ(z)/S(z)`, `h' = h(h-z)`) terms with freq
  weighting. Reproduces the 16 reference aVar entries from the ТЗ
  to ≤ 1e-9 across basic / cens / freq / both. Spec extended; gtest
  +4 TEST_F (13 total in suite); smoke updated.
