# stats/var — ТЗ for completion

**Status:** open
**Priority:** high
**Effort:** medium
**Audited at commit:** ba142e6
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/descriptive/descriptive.cpp:178` (`var`)
- Adapter: `libs/stats/src/descriptive/descriptive.cpp:920` (`var_reg`)
- Spec: `tools/parity/specs/var.json`
- What works today:
  - `V = var(A[, w[, dim]])` — scalar normFlag `w ∈ {0, 1}`,
    integer `dim`
  - `'omitnan'`/`'includenan'` trailing string (parsed by
    `stripNanFlag` helper)
  - Vector / 2-D / 3-D input

## MATLAB R2025b — actual behavior

Documented signatures (`help var`):

- `V = var(A)`
- `V = var(A, w)` — `w` is normFlag scalar OR a weight vector
- `V = var(A, w, "all")` — full-flatten reduction
- `V = var(A, w, dim)` — integer dim
- `V = var(A, w, vecdim)` — **vector of dims** to reduce simultaneously
- `V = var(___, nanflag)` — `'omitnan'` (default for `single`/
  `double` since R2023b) / `'includenan'`

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `var(A, 0, [1 2])` (vecdim) | reduce over dims 1+2 | adapter calls `args[2].toScalar()` ⇒ throws `Cannot convert double to scalar` | high |
| 2 | `var(A, 0, "all")` | full-flatten | throws `Cannot convert string to scalar` | high |
| 3 | `var(A, 0, 'all')` (char form) | same | likely throws same path | high |
| 4 | `var(A, W)` where `W` is a weight vector | weighted variance | numkit's `args[1].toScalar()` throws on vector | medium |
| 5 | default `nanflag` | `'omitnan'` (R2023b+) for floating-point | numkit default = `'includenan'`: `var(v) = NaN` when any v(i) is NaN | medium (silent default change for R2023b+ scripts) |

## Reference table (from probe)

Inputs:
```
A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11]   % 5x3
v = [2 5 3 7 4 6 NaN 8 1 9]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `var(A)` | `[2.5 2.5 2.5]` | identical ✅ |
| `var(A, 1)` | `[2 2 2]` | identical ✅ |
| `var(A, 0, 2)` | per-row, length 5 | identical ✅ |
| `var(A, 0, [1 2])` | `8.5714285714` | THROWS |
| `var(A, 0, "all")` | `8.5714285714` | THROWS |
| `var(v, 0, 'omitnan')` | `7.5` | `7.5` ✅ |
| `var(v, 0, 'includenan')` | `NaN` | `NaN` ✅ |
| default `var(v)` | `NaN` (legacy) or `7.5` (R2023b+ default) | `NaN` |

## Recommended fixes

1. **Accept vector `dim`:** when `args[2]` is a vector of integers,
   reduce over each named dimension in turn (matrix → row vector →
   scalar for `[1 2]`).
2. **Accept the `"all"`/`'all'` string flag** as `args[2]` (and
   any-position `args[1..n]`). When seen, flatten the array and
   compute on the full sample.
3. **Accept weight vector `W` for `args[1]`:** when `args[1]` is a
   vector of length matching `size(A, dim)`, treat as weights:
   `var = Σ w_i (x_i - μ)² / (Σ w_i)` (or `/ (Σ w_i - 1)` when
   normFlag=0).
4. **(Optional) Default-nanflag swap:** mirror MATLAB R2023b+'s
   change for floating-point inputs. Documented behaviour change —
   may break test fixtures, so flag separately.
5. **Adapter rewrite:** the gating logic currently is
   `args[2].toScalar()` first, which throws on vectors and strings.
   Switch to a type-dispatched path:
   ```cpp
   if (args[2].isChar() || args[2].isString())  -> "all" check
   else if (args[2].isVector())                 -> vecdim path
   else                                         -> scalar dim
   ```
6. **Spec extension:** add fingerprint entries for vecdim, "all",
   weighted, default+R2023b. `tol = 1e-9`.

## Out of scope for this ТЗ

- `var` on `single` precision still goes through the same C++
  path — extending numkit's narrow-output handling for vecdim is
  one extra `narrowToSingle` call.

## Closed
- Closed in commit: PENDING (joint var/std fix)
- Closed date: 2026-05-06
- Notes: Adapter rewritten via varStdDispatch helper. Supports 'all' string, full-flatten vecdim ([1 2] / [1 2 3]), and weight-vector W (denominator = sum(W)). Default nanflag remains 'includenan' (NaN poisons) — matches MATLAB R2025b documented default for double; auditor's R2023b-default-omitnan claim was incorrect (verified via probe). Partial vecdim and weight + non-flat dim not yet supported (documented).
