# stats/iqr — ТЗ for completion

**Status:** open
**Priority:** **high** (PROGRESS already flags `correctness=MISMATCH`)
**Effort:** small (joint with `quantile`/`prctile`)
**Audited at commit:** ba142e6
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/descriptive/descriptive_extras.cpp:80`
  (`iqr`)
- Adapter: `libs/stats/src/descriptive/descriptive_extras.cpp:638`
  (`iqr_reg`)
- Spec: `tools/parity/specs/iqr.json` (PROGRESS:
  `correctness=MISMATCH`)
- What works today:
  - `r = iqr(A[, dim])` — scalar dim
- Internally calls `quantile`/`prctile`, so inherits their
  Type-7-vs-MATLAB-R2007a interpolation mismatch.

## MATLAB R2025b — actual behavior

Documented signatures (`help iqr`):

- `r = iqr(A)`
- `r = iqr(A, "all")`
- `r = iqr(A, dim)`
- `r = iqr(A, vecdim)`

`iqr = prctile(A, 75) - prctile(A, 25)`. The MISMATCH in PROGRESS is
the same root cause as `quantile`/`prctile`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | underlying interpolation | R2007a positions | Type-7 (numkit) ⇒ off by ~one sample step on small N | high (PROGRESS MISMATCH) |
| 2 | `iqr(A, 'all')` | full-flatten | throws | high |
| 3 | `iqr(A, [1 2])` (vecdim) | reduce | throws | high |

## Reference table (from probe)

Inputs: `A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11]`

| Inputs | MATLAB | numkit |
|---|---|---|
| `iqr([1:10]')` | `5` | `4.5` ❌ (interpolation diff) |
| `iqr(A)` | `[2.5 2.5 2.5]` | `[2 2 2]` ❌ |
| `iqr(A, 'all')` | `4` | THROWS |
| `iqr(A, [1 2])` | `4` | THROWS |

## Recommended fixes

1. **Joint fix with `quantile`** (see
   `audit/findings/stats/quantile.md`). Once the underlying
   interpolation switches to the R2007a algorithm, `iqr` numbers
   align automatically.
2. **Adapter rewrite:** type-dispatch on `args[1]` for `'all'`,
   vector, scalar (same shape as `bounds`/`var`/`std`).
3. **Spec extension:** PROGRESS notes `correctness=MISMATCH`;
   regenerate `iqr.json` after the algorithm fix.

## Out of scope for this ТЗ

- N/A — joint fix.
