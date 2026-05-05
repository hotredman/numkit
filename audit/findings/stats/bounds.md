# stats/bounds — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** ba142e6
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/descriptive/descriptive_extras.cpp:571`
  area (`bounds`)
- Adapter: `libs/stats/src/descriptive/descriptive_extras.cpp:627`
  (`bounds_reg`)
- Spec: `tools/parity/specs/bounds.json`
- What works today:
  - `[lo, hi] = bounds(A[, dim])` — scalar dim

## MATLAB R2025b — actual behavior

Documented signatures (`help bounds`):

- `[lo, hi] = bounds(A)`
- `[lo, hi] = bounds(A, "all")`
- `[lo, hi] = bounds(A, dim)`
- `[lo, hi] = bounds(A, vecdim)`
- `[lo, hi] = bounds(___, missingflag)`

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `bounds(A, "all")` / `'all'` | full-flatten | `args[1].toScalar()` ⇒ throws `Cannot convert char to scalar` | high |
| 2 | `bounds(A, [1 2])` (vecdim) | reduce | throws `Cannot convert double to scalar` | high |
| 3 | `bounds(___, 'omitnan')` / `'includenan'` | NaN handling | not supported | medium |

## Reference table (from probe)

Inputs: `A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11]`

| Inputs | MATLAB | numkit |
|---|---|---|
| `[lo,hi] = bounds([1 4 7; 2 5 8; 3 6 9])` | `lo=[1 4 7], hi=[3 6 9]` | identical ✅ |
| `bounds(A, 'all')` | `lo=1, hi=11` | THROWS |
| `bounds(A, [1 2])` | `lo=1, hi=11` | THROWS |

## Recommended fixes

1. **Adapter rewrite (same shape as var/std/median):** type-dispatch
   on `args[1]` for `"all"`/`'all'`, vector dim, scalar dim, and the
   missingflag string.
2. **Add nanflag support:** `'omitnan'` skips NaN before reduction;
   `'includenan'` keeps it (so any NaN in slice ⇒ output NaN).
3. **Spec extension:** add fingerprint for "all", vecdim, omitnan.
   `tol = 0`.

## Out of scope for this ТЗ

- `bounds` on complex data — MATLAB's behaviour is by `abs`; can be
  added with a follow-up if needed.
