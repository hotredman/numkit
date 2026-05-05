# stats/mode — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** ba142e6
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/descriptive/descriptive.cpp` (`mode`,
  ~line 571 area)
- Adapter: `libs/stats/src/descriptive/descriptive.cpp:1054`
  (`mode_reg`)
- Spec: `tools/parity/specs/mode.json`
- What works today:
  - `M = mode(A[, dim])` — scalar dim
  - `[M, F] = mode(A, ...)` 2-output (mode + frequency)

## MATLAB R2025b — actual behavior

Documented signatures (`help mode`):

- `M = mode(A)` / `(A, 'all')` / `(A, dim)` / `(A, vecdim)`
- `[M, F] = mode(___)` / `[M, F, C] = mode(___)` — `C` is a cell
  array of all tied modes per slice

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `mode(A, 'all')` | full-flatten | adapter `args[1].toScalar()` ⇒ throws `Cannot convert char to scalar` | high |
| 2 | `mode(A, [1 2])` (vecdim) | reduce over dims | throws `Cannot convert double to scalar` | high |
| 3 | `[M, F, C] = mode(...)` 3rd output | cell of all tied modes | not produced | low |

## Reference table (from probe)

Inputs: `A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11]`

| Inputs | MATLAB | numkit |
|---|---|---|
| `mode([1 2 2 3 3 3 4 4 4 4]')` | `4` | `4` ✅ |
| `mode(A)` | `[1 4 7]` (lowest in each tied column) | `[1 4 7]` ✅ |
| `mode(A, 'all')` | `4` (lowest tied across whole array) | THROWS |
| `mode(A, [1 2])` | `4` | THROWS |

## Recommended fixes

1. **Adapter rewrite:** type-dispatch on `args[1]`:
   ```
   if (isChar/String && lower(s)=='all')   -> full-flatten branch
   else if (isVector)                       -> vecdim branch
   else                                     -> scalar dim
   ```
2. **Implement vecdim and 'all' branches** — flatten to a single
   vector, run the existing mode algorithm.
3. **(Optional) Add 3rd output `C`** as a cell of tied modes per
   reduction slice. Low priority; very few scripts use it.
4. **Spec extension:** add fingerprint for 'all', vecdim, 2nd output
   `F`. `tol = 0` (integer-stable).

## Out of scope for this ТЗ

- `mode` on `categorical`/`string` data — needs OOP type support.
