# stats/evlike — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:401` (`evlike`)
- Adapter: `libs/stats/src/fit/fit.cpp:613` (`evlike_reg` → `like2_reg`)
- Spec: `tools/parity/specs/evlike.json`
- What works today:
  - `nL = evlike([mu sigma], data)` — Type-I extreme-value (Gumbel min)
  - Returns `+inf` for `sigma <= 0` or empty input

## MATLAB R2025b — actual behavior

Documented signatures (`help evlike`):

- `nlogL = evlike(params, data)`
- `[nlogL, AVAR] = evlike(params, data)` — 2×2 inverse-observed-Fisher
- `[...] = evlike(params, data, censoring)`
- `[...] = evlike(params, data, censoring, freq)`

Edge convention: `sigma <= 0` ⇒ `NaN`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `evlike(params, data, cens)` | right-censored `-log(S(z))` | not supported | medium |
| 2 | `evlike(params, data, [], freq)` | freq-weighted | not supported | medium |
| 3 | `evlike(params, data, cens, freq)` | combined | not supported | medium |
| 4 | `[nL, AVAR] = evlike(...)` | 2×2 matrix | not produced | medium |
| 5 | invalid params | returns `NaN` | returns `+inf` | high |

## Reference table (from probe)

Inputs:
```
data = [1 2 3 4 5]'
cens = [0 0 0 1 1]'
freq = [2 2 1 1 1]'
params = [0, 1]
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `nL = evlike(params, data)` | `218.2041839863` | `218.2041839863` ✅ |
| `AVAR(1,1)` basic | `-0.7537259703` | (not produced) |
| `AVAR(1,2)` = `AVAR(2,1)` basic | `0.1395845503` | (not produced) |
| `AVAR(2,2)` basic | `-0.0257038065` | (not produced) |
| `nL = evlike(params, data, cens)` | `227.2041839863` | `218.2041839863` ❌ |
| `nL = evlike(params, data, [], freq)` | `225.3115219137` | `218.2041839863` ❌ |
| `nL = evlike(params, data, cens, freq)` | `234.3115219137` | `218.2041839863` ❌ |

## Recommended fixes

1. **Extend signature to 4 args.** For Gumbel-min the survival is
   `S(z) = 1 - exp(-exp(z))` with `z = (x-mu)/sigma`; censored
   contribution is `-log(S(z(i)))`. Use table values as contract.
2. **Add second output `AVAR`** — 2×2 inverse-observed-Fisher matrix
   (parameter order `[mu sigma]`). Reflect cens/freq weighting.
3. **Edge convention:** `sigma <= 0` ⇒ `NaN` (not Inf).
4. **Spec extension:** fingerprint
   `[nL_basic, nL_cens, nL_freq, nL_both,`
   ` av_basic(1,1), av_basic(1,2), av_basic(2,2)]`.
5. **Adapter:** dedicated `evlike_reg` accepting up to 4 args; remove
   from `like2_reg` dispatcher or generalise the shared helper.
6. **PROGRESS.md row update:** drop *"Default-path only"* trailing
   clause.

## Out of scope for this ТЗ

- Type-I extreme value of the **maximum** form (Gumbel-max) — MATLAB
  uses Gumbel-min as the documented `evlike`, this stays the contract.

## Closed (partial)
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Closed gaps #1, #2, #3, #5. Deferred gap #4 (AVAR).

  **Implemented:**
  1. Censored: `-log S(z) = exp(z)` per row, weight w (gap #1).
  2. Frequency: weight uncensored contribution by `freq(i)` (gap #2).
  3. Combined cens + freq (gap #3).
  4. Edge fix: σ <= 0 -> NaN (was +Inf); empty data -> 0 (was +Inf).
     Both match MATLAB R2025b convention (gap #5).

  **Deferred — gap #4 (AVAR / 2-output form):** observed Fisher info
  for Gumbel-min has nontrivial mixed partials:
    H_μμ = -e^z/σ², H_μσ = (e^z(1+z)−1)/σ², H_σσ = (-1−2z+2z·e^z+z²·e^z)/σ².
  A separate ТЗ should derive + verify these against MATLAB. (When
  evaluated away from the MLE, AVAR can have negative diagonal
  entries — MATLAB returns those as-is, so any future implementation
  must match that quirk.) Tracking: file new ТЗ if `paramci`/`fitdist`
  consumers start needing it.

  Spec extended from 1 to 7 fingerprints (4 modes + 2 invalid σ +
  empty). Parity OK numkit ↔ MATLAB at tol=1e-9. Octave's evlike
  errors on empty data; we follow MATLAB. 6 TEST_F gtest + smoke.
