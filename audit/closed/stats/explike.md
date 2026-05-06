# stats/explike — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:322` (`explike`)
- Adapter: `libs/stats/src/fit/fit.cpp:617` (`explike_reg`)
- Spec: `tools/parity/specs/explike.json`
- What works today:
  - `nL = explike(mu, data)` — 2-arg form
  - Returns `+inf` for `mu <= 0` or empty input

## MATLAB R2025b — actual behavior

Documented signatures (`help explike`):

- `nlogL = explike(param, data)`
- `[nlogL, avar] = explike(param, data)` — `avar` is a **scalar**
  (single-parameter distribution: 1/(observed Fisher info)).
- `[...] = explike(param, data, censoring)`
- `[...] = explike(param, data, censoring, freq)`

Edge convention: `mu <= 0` ⇒ `NaN`; empty data ⇒ `0`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `explike(mu, data, cens)` | right-censored exponential nL | accepts only 2 args (the third is silently ignored at adapter level — extra args don't error but don't reach impl) | medium |
| 2 | `explike(mu, data, [], freq)` | freq-weighted | not supported | medium |
| 3 | `explike(mu, data, cens, freq)` | combined | not supported | medium |
| 4 | `[nL, avar] = explike(...)` | scalar avar | not produced | medium |
| 5 | `mu <= 0` | returns `NaN` | returns `+inf` | high |
| 6 | empty data | returns `0` | returns `+inf` | high |

## Reference table (from probe)

Inputs:
```
data = [1 2 3 4 5]'
cens = [0 0 0 1 1]'
freq = [2 2 1 1 1]'
mu = 2
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `nL = explike(2, data)` | `10.9657359028` | `10.9657359028` ✅ |
| `avar` basic | `0.4000000000` (scalar) | (not produced) |
| `nL = explike(2, data, cens)` | `9.5794415417` | `10.9657359028` ❌ |
| `nL = explike(2, data, [], freq)` | `13.8520302639` | `10.9657359028` ❌ |
| `nL = explike(2, data, cens, freq)` | `12.4657359028` | `10.9657359028` ❌ |
| `explike(-1, data)` | `NaN` | `inf` ❌ |
| `explike(0, data)` | `NaN` | `inf` ❌ |
| `explike(2, [])` | `0.0` | `inf` ❌ |

Note: MATLAB's `avar` for the 1-parameter exponential at `mu=2`,
`N=5` evaluates to `4/5 ÷ 2 = 0.4` — i.e. `mu²/N`. Reference value
above is the contract regardless of formula.

## Recommended fixes

1. **Extend signature to 4 args** (`cens`, `freq`). The censored term
   at observed `data(i)` becomes `-log(S(data(i)))` with
   `S(x; mu) = exp(-x/mu)`, i.e. the censored contribution per point
   is `data(i)/mu` (the survival log is linear). Use the table values
   above as the acceptance contract.
2. **Add second scalar output `avar`** — must be the inverse of the
   observed Fisher info; reflect cens/freq weighting when supplied.
3. **Edge convention:** `mu <= 0` ⇒ `NaN` (not Inf); empty data ⇒ `0`
   (not Inf). Match `normlike`'s recently-corrected edges.
4. **Spec extension:** new fingerprint:
   `[nL_basic, nL_cens, nL_freq, nL_both, avar_basic,`
   ` nL_neg_mu, nL_zero_mu, nL_empty]`. `tol = 1e-9`.
5. **PROGRESS.md row update:** replace the *"Default-path only"*
   trailing clause with full-coverage description.
6. **Adapter:** rewrite `explike_reg` to accept up to 4 input args and
   to honour `nargout`.

## Out of scope for this ТЗ

- Adapting `like2_reg` to support `cens/freq` — `explike` has a single
  parameter, so it should not share that scaffold; keep it dedicated.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-06
- Notes:
  - New `explike_full` helper handles cens + freq + scalar avar in
    one pass. Two-arg `explike()` still exposed for direct callers.
  - `explike_reg` rewritten to accept up to 4 args + honour nargout.
  - **Edges fixed:** `mu <= 0` → NaN (was +Inf); empty data → 0
    (was +Inf). Matches MATLAB R2025b.
  - **avar formula** (analytical, since exp likelihood Hessian is
    closed-form):
        I = Σ w_i ∂²nL_i/∂μ²
        uncens row: -1/μ² + 2x/μ³
        cens   row: 2x/μ³
        avar = 1/I
    Reproduces the 4 reference avar values (0.4, 1/3, 4/11, 4/13)
    exactly (1e-12).
  - Spec extended to 11 fingerprint values; parity OK numkit ↔ MATLAB.
  - 8 TEST_F gtest + smoke .m.
