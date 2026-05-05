# stats/betalike — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:366` (`betalike`)
- Adapter: `libs/stats/src/fit/fit.cpp:605` (`betalike_reg` → `like2_reg`)
- Spec: `tools/parity/specs/betalike.json`
- What works today:
  - `nL = betalike([a b], data)`
  - Returns `+inf` for `a<=0`, `b<=0`, or any `data(i) <= 0` / `>= 1`

## MATLAB R2025b — actual behavior

Documented signatures (`help betalike`):

- `nlogL = betalike(params, data)`
- `[nlogL, AVAR] = betalike(params, data)` — 2×2 inverse-observed-Fisher

`betalike` does **not** document `cens`/`freq`.

Edge convention: invalid params or out-of-(0,1) data ⇒ `NaN`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `[nL, AVAR] = betalike(params, data)` | 2×2 matrix | not produced | medium |
| 2 | `betalike([2, 2], [0.5 1.5]')` (x>1) | `NaN` | `+inf` | high |
| 3 | `betalike([0, 2], data)` (a=0) | `NaN` | `+inf` | high |

## Reference table (from probe)

Inputs:
```
data = [0.1 0.3 0.5 0.7 0.9]'
params = [a=2, b=2]
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `nL = betalike(params, data)` | `0.3646837288` | `0.3646837288` ✅ |
| `AVAR(1,1)` basic | `0.9234392430` | (not produced) |
| `AVAR(1,2)` = `AVAR(2,1)` basic | `0.7431196647` | (not produced) |
| `AVAR(2,2)` basic | `0.9234392430` | (not produced) |
| `betalike(params, [0.5 1.5]')` | `NaN` | `inf` ❌ |
| `betalike([0, 2], data)` | `NaN` | `inf` ❌ |

Note: AVAR is symmetric (1,1)=(2,2) here because `params=[2,2]` is the
symmetric Beta — informative confirmation that the contract is the
**observed** Fisher info, not the expected.

## Recommended fixes

1. **Add second output `AVAR`** — 2×2 inverse-observed-Fisher matrix
   (parameter order `[a b]`). The closed form requires the trigamma
   function `psi'`; if no helper exists, fall back to a numerical
   Hessian as in the recommended `gamlike` ТЗ. Acceptance via the
   reference values.
2. **Edge convention:** invalid params or out-of-`(0,1)` data ⇒ `NaN`
   (not Inf).
3. **Spec extension:** fingerprint
   `[nL_basic, av_basic(1,1), av_basic(1,2), av_basic(2,2),`
   ` nL_x_oor, nL_a0]`. `tol = 1e-9` (or `1e-7` for finite-diff AVAR).
4. **PROGRESS.md row update:** drop *"Default-path only"* clause; note
   that MATLAB doesn't document `cens`/`freq` for `betalike`.

## Out of scope for this ТЗ

- `cens`/`freq` — not documented for MATLAB's `betalike`.
- The `betafit` companion (currently ❌ in PROGRESS) — separate work.
