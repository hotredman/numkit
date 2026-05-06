# stats/lognlike — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:331` (`lognlike`)
- Adapter: `libs/stats/src/fit/fit.cpp:597` (`lognlike_reg` → `like2_reg`)
- Spec: `tools/parity/specs/lognlike.json`
- What works today:
  - `nL = lognlike([mu sigma], data)` — 2-arg form only
  - Returns `+inf` for empty input, `sigma <= 0`, or any `x <= 0`

## MATLAB R2025b — actual behavior

Documented signatures (`help lognlike`):

- `nlogL = lognlike(params, x)`
- `nlogL = lognlike(params, x, censoring)`
- `nlogL = lognlike(params, x, censoring, freq)`
- `[nlogL, aVar] = lognlike(___)` — inverse of observed Fisher info
  (2×2 matrix, parameter order `[mu sigma]`).

Edge convention: invalid `sigma <= 0` ⇒ `NaN`; non-positive entries
in `x` ⇒ `NaN`. Empty data ⇒ `0`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `lognlike(params, x, censoring)` | right-censors via `-log(S(z))` on lognormal scale | `like2_reg` only forwards 2 args; further args silently ignored | medium |
| 2 | `lognlike(params, x, [], freq)` | weights each contribution by `freq(i)` | not supported | medium |
| 3 | `lognlike(params, x, cens, freq)` | combined | not supported | medium |
| 4 | `[nL, aVar] = lognlike(...)` | 2×2 inverse-observed-Fisher matrix | second output not produced | medium |
| 5 | invalid params / data | returns `NaN` | returns `+inf` | high |

## Reference table (from probe)

Inputs:
```
x    = [1 2 3 4 5 6 7 8 9 10]'
cens = [0 0 0 0 0 0 0 1 1 1]'
freq = [2 2 2 1 1 1 1 1 1 1]'
params = [0, 1]
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `nL = lognlike(params, x)` | `38.1189198087` | `38.1189198087` ✅ |
| `aVar(1,1)` basic | `-0.3984945873` | (not produced) |
| `aVar(1,2)` = `aVar(2,1)` basic | `0.1650162113` | (not produced) |
| `aVar(2,2)` basic | `-0.0546251668` | (not produced) |
| `nL = lognlike(params, x, cens)` | `34.3411160493` | `38.1189198087` ❌ (cens ignored) |
| `nL = lognlike(params, x, [], freq)` | `43.5111958649` | `38.1189198087` ❌ (freq ignored) |
| `nL = lognlike(params, x, cens, freq)` | `39.7333921055` | `38.1189198087` ❌ |
| `lognlike([0, 0], [1 2 3 4 5]')` (sigma=0) | `NaN` | `inf` ❌ |
| `lognlike([0, 1], [-1 2 3]')` (x<0) | `NaN` | `inf` ❌ |

Note: MATLAB's basic `aVar` values include negative diagonal entries —
this is the **observed** Fisher info, not the expected; it can be
indefinite if `params` is far from the MLE. Not a bug, just a
characteristic of the API contract.

## Recommended fixes

1. **Extend signature to 4 args (`cens`, `freq`)** matching `normlike`'s
   precedent. The censored term for lognormal at observed-data point
   `xi > 0` and `params = [mu sigma]` mirrors the normal-on-log-x
   convention: replace the per-point density with
   `-log(S(log(xi); mu, sigma))`. Use the table values above as the
   acceptance contract — implementation must match within `tol = 1e-9`.
2. **Add second output `aVar`** — 2×2 inverse-observed-Fisher matrix
   (parameter order `[mu sigma]`). Must reflect cens/freq weighting if
   they are passed. Acceptance via probe values.
3. **Edge convention swap:** invalid `sigma <= 0` ⇒ `NaN` (not Inf);
   non-positive `x(i)` ⇒ `NaN` (not Inf); empty data ⇒ `0`. This brings
   the function in line with MATLAB and with the recently-fixed
   `normlike`.
4. **Spec extension:** mirror `normlike.json`'s pattern. New
   fingerprint:
   `[nL_basic, nL_cens, nL_freq, nL_both,`
   ` nL_sig0, nL_xneg, nL_empty,`
   ` av_basic(1,1), av_basic(1,2), av_basic(2,2)]`.
   `tol = 1e-9`.
5. **PROGRESS.md row update:** replace the trailing
   *"Default-path only — no `freq`, `censoring`, or `avar` second output."*
   with the new full-coverage description.
6. **Adapter:** `lognlike_reg` currently delegates to `like2_reg`, which
   accepts only `(params, data)`. Either generalise `like2_reg` or
   replace with a dedicated `lognlike_reg` modelled on `normlike_reg`.

## Out of scope for this ТЗ

- Per-point clipping of log(x) for very small x — leave to future BUGS
  ticket if numerical stability surfaces.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-06
- Notes:
  - New `lognlike_full` helper handles cens + freq + 2×2 aVar in one
    pass. Hessian wrt (mu, sigma) is structurally identical to the
    normal Hessian on y=log x (the per-row `log x_i` baseline is a
    constant in (mu, sigma)). Same uncensored / right-censored split
    as normlike (h=φ(z)/S(z), h'=h(h-z) for censored rows).
  - `lognlike_reg` rewritten standalone (no longer via `like2_reg`).
  - **Edges fixed (matching MATLAB R2025b):** sigma<=0 => NaN,
    x<=0 => NaN, empty data => 0 (was +Inf in all three).
  - Reproduces all 4 reference nL values (basic / cens / freq / both)
    and all 3 aVar entries to ≤ 1e-9. aVar can be non-PD at non-MLE
    params (observed Fisher, not expected) — the reference's negative
    diagonals are matched.
  - Spec extended (10 fingerprints); parity OK numkit ↔ MATLAB.
  - 9 TEST_F gtest + smoke .m.
