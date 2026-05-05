# stats/gamlike — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:349` (`gamlike`)
- Adapter: `libs/stats/src/fit/fit.cpp:601` (`gamlike_reg` → `like2_reg`)
- Spec: `tools/parity/specs/gamlike.json`
- What works today:
  - `nL = gamlike([a b], data)` — shape `a`, scale `b`
  - Returns `+inf` for `a<=0`, `b<=0`, or `data(i)<=0`

## MATLAB R2025b — actual behavior

Documented signatures (`help gamlike`):

- `nlogL = gamlike(params, data)`
- `[nlogL, AVAR] = gamlike(params, data)` — 2×2 inverse-observed-Fisher

`gamlike` does **not** document `cens`/`freq` (unlike most peers).

Edge convention: invalid params ⇒ `NaN`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `[nL, AVAR] = gamlike(params, data)` | 2×2 matrix | not produced | medium |
| 2 | `gamlike([0, 1], data)` | `NaN` | `+inf` | high |
| 3 | `gamlike([2, 0], data)` | `NaN` | `+inf` | high |

## Reference table (from probe)

Inputs:
```
data = [1 2 3 4 5]'
params = [a=2, b=1]
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `nL = gamlike(params, data)` | `10.2125082572` | `10.2125082572` ✅ |
| `AVAR(1,1)` basic | `0.5064136442` | (not produced) |
| `AVAR(1,2)` = `AVAR(2,1)` basic | `-0.1266034110` | (not produced) |
| `AVAR(2,2)` basic | `0.0816508528` | (not produced) |
| `gamlike([0, 1], data)` | `NaN` | `inf` ❌ |
| `gamlike([2, 0], data)` | `NaN` | `inf` ❌ |

## Recommended fixes

1. **Add second output `AVAR`** — 2×2 inverse-observed-Fisher matrix
   (parameter order `[a b]`). For Gamma the Hessian involves the
   trigamma function `psi'(a)`; if no in-tree `polygamma` helper
   exists, use a numerical Hessian (central differences in log-param
   space) and document the choice. Acceptance via the values above.
2. **Edge convention:** invalid params ⇒ `NaN` (not Inf).
3. **Spec extension:** fingerprint
   `[nL_basic, av_basic(1,1), av_basic(1,2), av_basic(2,2),`
   ` nL_a0, nL_b0]`. `tol = 1e-9` for `nL`; `tol = 1e-7` may be needed
   if `AVAR` is computed via finite differences.
4. **PROGRESS.md row update:** drop *"Default-path only — no `freq`,
   `censoring`, or `avar` second output"* clause; new comment notes
   that MATLAB does not document `cens`/`freq` here, only `avar`.

## Out of scope for this ТЗ

- `cens`/`freq` — not documented for MATLAB's `gamlike`.
- The `gamfit` companion (currently ❌ in PROGRESS) — separate work.
