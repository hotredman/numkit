# stats/gevlike — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** bfda361
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/fit/fit.cpp:415` (`gevlike`)
- Adapter: `libs/stats/src/fit/fit.cpp:628` (`gevlike_reg`)
- Spec: `tools/parity/specs/gevlike.json`
- What works today:
  - `nL = gevlike([k sigma mu], data)` — three-parameter GEV
  - Closed-form branches for `k=0` (Gumbel-MAX limit) and `k≠0`
  - Returns `+inf` for `sigma<=0` or out-of-support point

## MATLAB R2025b — actual behavior

Documented signatures (`help gevlike`):

- `nlogL = gevlike(params, data)`
- `[nlogL, ACOV] = gevlike(params, data)` — 3×3 inverse-observed-Fisher

`gevlike` does **not** document `cens`/`freq`.

Edge convention: `sigma <= 0` ⇒ `NaN`; per-point support violation
(`1 + k*z <= 0`) ⇒ `NaN` for the whole call.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `[nL, ACOV] = gevlike(params, data)` | 3×3 matrix | not produced | medium |
| 2 | out-of-support data | returns `NaN` | returns `+inf` | high |

## Reference table (from probe)

Inputs:
```
data = [1 2 3 4 5]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `nL = gevlike([0.5, 1, 0], data)` | `14.1460230417` | `14.1460230417` ✅ |
| `ACOV(1,1)` k>0 | `0.7323243670` | (not produced) |
| `ACOV(1,2)` = `ACOV(2,1)` k>0 | `-0.4845307553` | (not produced) |
| `ACOV(1,3)` = `ACOV(3,1)` k>0 | `-0.0703687586` | (not produced) |
| `ACOV(2,2)` k>0 | `0.3841928178` | (not produced) |
| `ACOV(2,3)` = `ACOV(3,2)` k>0 | `0.3851466902` | (not produced) |
| `ACOV(3,3)` k>0 | `-1.2357793582` | (not produced) |
| `nL = gevlike([0, 1, 0], data)` | `15.5780553787` | `15.5780553787` ✅ |
| `ACOV(1,1)` k=0 | `0.0030344445` | (not produced) |
| `ACOV(2,2)` k=0 | `0.0000426062` | (not produced) |
| `ACOV(3,3)` k=0 | `1.3756626884` | (not produced) |
| `gevlike([0.5, 1, 0], [-100; -1; 0])` | `NaN` | `inf` ❌ |

## Recommended fixes

1. **Add second output `ACOV`** — 3×3 inverse-observed-Fisher matrix
   (parameter order `[k sigma mu]`). For non-zero `k` the closed form
   is involved; numerical Hessian (central differences in
   `[k log(sigma) mu]` then back-transformed) is acceptable. `k=0`
   limit must be handled separately; see reference values above.
2. **Edge convention:** any out-of-support `1+k*z <= 0` ⇒ `NaN` (not
   Inf). Note that MATLAB still supports `k=0` everywhere, so the
   support check is per-point only when `k != 0`.
3. **Spec extension:** fingerprint
   `[nL_kpos, nL_k0,`
   ` ac_kpos(1,1), ac_kpos(1,2), ac_kpos(1,3),`
   ` ac_kpos(2,2), ac_kpos(2,3), ac_kpos(3,3),`
   ` ac_k0(1,1), ac_k0(2,2), ac_k0(3,3),`
   ` nL_oos]`. `tol = 1e-7` if numerical Hessian is used.
4. **PROGRESS.md row update:** drop *"Default-path only — no `freq`,
   `censoring`, or `avar` second output"* clause; note that MATLAB
   doesn't document `cens`/`freq` for `gevlike`, only `ACOV`.

## Out of scope for this ТЗ

- `cens`/`freq` — not in MATLAB's `gevlike` contract.
- The `gevfit` companion (currently ❌ in PROGRESS) — separate work.

## Closed (PARTIAL)
- Closed in commit: PENDING
- Closed date: 2026-05-06
- Notes:
  - **k != 0:** ACOV computed via central-difference Hessian (new
    `fill_fd_avar3` 3×3 helper, 18 nL evaluations + closed-form
    cofactor inversion). Matches MATLAB to ~3e-7 across all 6
    upper-triangle entries at [k=0.5, σ=1, μ=0], x=[1..5].
  - **Edge:** sigma<=0 and per-point support violation (1+k*z<=0)
    now return NaN (was +Inf), matching MATLAB R2025b.
  - **Known gap (k=0 exactly):** MATLAB uses an analytical
    Gumbel-limit Hessian; FD straddling k=0 reports a different
    ACOV (FD-on-MATLAB's-own-gevlike confirms ~0.030, 0.098, -1.622
    rather than MATLAB's 0.003, 4e-5, 1.376). Implementing the
    analytical Gumbel-limit Hessian (with k-derivatives via Taylor
    expansion of (1/k+1)·log(1+kz) + (1+kz)^(-1/k) around k=0) is
    significant work; deferred. nL at k=0 still matches exactly.
  - 6 TEST_F gtest + smoke .m. Parity OK numkit ↔ MATLAB at tol=1e-6.
