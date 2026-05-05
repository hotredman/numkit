# stats/wbllike — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:384` (`wbllike`)
- Adapter: `libs/stats/src/fit/fit.cpp:609` (`wbllike_reg` → `like2_reg`)
- Spec: `tools/parity/specs/wbllike.json`
- What works today:
  - `nL = wbllike([scale shape], data)` — 2-arg form
  - Returns `+inf` for `scale<=0`, `shape<=0`, or any `data(i)<=0`

## MATLAB R2025b — actual behavior

Documented signatures (`help wbllike`):

- `nlogL = wbllike(params, data)`
- `[logL, AVAR] = wbllike(params, data)` — 2×2 inverse-observed-Fisher
- `[...] = wbllike(params, data, censoring)`
- `[...] = wbllike(params, data, censoring, freq)`

Edge convention: invalid params ⇒ `NaN`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `wbllike(params, data, cens)` | right-censored | not supported | medium |
| 2 | `wbllike(params, data, [], freq)` | freq-weighted | not supported | medium |
| 3 | `wbllike(params, data, cens, freq)` | combined | not supported | medium |
| 4 | `[nL, AVAR] = wbllike(...)` | 2×2 matrix | not produced | medium |
| 5 | invalid `scale` / `shape` | returns `NaN` | returns `+inf` | high |

## Reference table (from probe)

Inputs:
```
data = [1 2 3 4 5]'
cens = [0 0 0 1 1]'
freq = [2 2 1 1 1]'
params = [scale=1, shape=2]
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `nL = wbllike(params, data)` | `46.7467723544` | `46.7467723544` ✅ |
| `AVAR(1,1)` basic | `-0.0218712228` | (not produced) |
| `AVAR(1,2)` = `AVAR(2,1)` basic | `-0.0399632819` | (not produced) |
| `AVAR(2,2)` basic | `-0.0638922093` | (not produced) |
| `nL = wbllike(params, data, cens)` | `51.1287989891` | `46.7467723544` ❌ |
| `nL = wbllike(params, data, [], freq)` | `49.6673308127` | `46.7467723544` ❌ |
| `nL = wbllike(params, data, cens, freq)` | `54.0493574474` | `46.7467723544` ❌ |
| `wbllike([0, 2], data)` (scale=0) | `NaN` | `inf` ❌ |

## Recommended fixes

1. **Extend signature to 4 args.** Weibull survival is
   `S(x) = exp(-(x/scale)^shape)`; censored contribution per point is
   `(data(i)/scale)^shape`. Use table values as contract.
2. **Add second output `AVAR`** — 2×2 inverse-observed-Fisher
   (parameter order `[scale shape]`). Reflect cens/freq weighting.
3. **Edge convention:** invalid params ⇒ `NaN` (not Inf).
4. **Spec extension:** fingerprint
   `[nL_basic, nL_cens, nL_freq, nL_both,`
   ` av_basic(1,1), av_basic(1,2), av_basic(2,2)]`.
5. **Adapter:** dedicated `wbllike_reg` accepting up to 4 args.
6. **PROGRESS.md row update:** drop *"Default-path only"* clause.

## Out of scope for this ТЗ

- 3-parameter Weibull (location shift) — MATLAB `wbllike` is
  2-parameter; the location form is not part of this contract.
