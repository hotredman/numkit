# wavelet/detcoef — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** 0e895fe
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/dwt/multilevel.cpp:176` (`detcoef`)
- Adapter: `libs/wavelet/src/dwt/multilevel.cpp:255` (`detcoef_reg`)
- Spec: `tools/parity/specs/detcoef.json`
- What works today:
  - `D = detcoef(C, L, level)` — extract detail at given level

## MATLAB R2025b — actual behavior

Documented signatures (`help detcoef`):

- `D = detcoef(C, L)` — **default level = 1** (finest)
- `D = detcoef(C, L, N)` — level N
- `D = detcoef(C, L, N, "cells")` — N is a vector; output is a
  cell array of details per requested level

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `detcoef(C, L)` without level | default level=1 | numkit throws "detcoef: requires (C, L, level)" | high |
| 2 | `detcoef(C, L, [1 2 3], 'cells')` cells form | returns a cell of detail vectors | not supported | medium |
| 3 | numeric values | follow MATLAB dwt convention | inherits dwt mismatch | medium (cascade) |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `detcoef(c, l)` | finest detail (level=1) | THROWS |
| `detcoef(c, l, 1)` | finest detail | works ✅ |
| `detcoef(c, l, 1:3, 'cells')` | 3-element cell of details | THROWS |

## Recommended fixes

1. **Default `level = 1`** when `args.size() == 2`.
2. **Implement `'cells'` form:** when `args[2]` is a vector and
   `args[3] == 'cells'`, return a cell of detail vectors (one per
   requested level).
3. **Cascade: numeric values** become correct after dwt/idwt fix.
4. **Spec extension:** add fingerprint for default-level, vector-
   level, cells. `tol = 1e-12`.

## Out of scope for this ТЗ

- The `'cells'` form needs cell-array support in `Value`. Numkit
  has cells; the integration is purely adapter-level.
