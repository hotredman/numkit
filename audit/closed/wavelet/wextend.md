# wavelet/wextend — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** 0e895fe
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/dwt/wkeep_wextend.cpp:131` (`wextend_reg`)
- Spec: `tools/parity/specs/wextend.json`
- What works today:
  - `Y = wextend(1, mode, x, lf[, side])` — 1-D extension
  - Modes: `'sym'`, `'per'`, `'zpd'`, `'ppd'`
  - `side` ∈ `'b'` (both, default), `'l'`, `'r'`

## MATLAB R2025b — actual behavior

Documented signatures (`help wextend`):

- `yext = wextend(type, mode, x, len)`
- `yext = wextend(___, loc)` — 'b'/'l'/'r' (1-D)

`type` selects 1 (1-D), 2 (2-D), 'ar' (along row), 'ac' (along col).
`mode` selects extension method:
- `'sym'` / `'symw'` — symmetric (whole-point or with-edge)
- `'asym'` / `'asymw'` — antisymmetric variants
- `'zpd'` — zero pad
- `'sp0'` / `'sp1'` — order-0 / order-1 spline extrapolation
- `'ppd'` — true periodic
- `'per'` — periodic with edge-pad on odd N

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `'symw'`, `'asym'`, `'asymw'`, `'sp0'`, `'sp1'` modes | distinct extensions | not supported (would throw or fall through) | medium |
| 2 | type=2 (2-D) | extends 2-D matrix | not supported | medium |
| 3 | type='ar' / 'ac' | along-row/col only | not supported | medium |

## Reference table (from probe)

Inputs: `x = (1:5)'`, `lf = 2`

| Inputs | MATLAB | numkit |
|---|---|---|
| `wextend(1, 'sym', x, 2)` | `[2 1 1 2 3 4 5 5 4]` | identical ✅ |
| `wextend(1, 'per', x, 2)` | `[5 5 1 2 3 4 5 5 1 2]` | identical ✅ |
| `wextend(1, 'zpd', x, 2)` | `[0 0 1 2 3 4 5 0 0]` | identical ✅ |
| `wextend(1, 'ppd', x, 2)` | `[4 5 1 2 3 4 5 1 2]` | identical ✅ |
| `wextend(1, 'sym', x, 2, 'l')` | `[2 1 1 2 3 4 5]` | identical ✅ |
| `wextend(1, 'sym', x, 2, 'r')` | `[1 2 3 4 5 5 4]` | identical ✅ |
| `wextend(2, 'sym', M, 1)` (2-D) | extends matrix | not supported (gap) |

## Recommended fixes

1. **Implement remaining modes:** `'symw'` (symmetric-with-edge,
   doesn't repeat the boundary sample), `'asym'`/`'asymw'` (sign-
   negated reflections), `'sp0'` (replicate edge sample), `'sp1'`
   (linear extrapolation), and the spline forms.
2. **Implement type=2 (2-D):** apply the 1-D extension to each
   row, then to each column (or per `loc` row/col select).
3. **Implement type='ar'/'ac':** apply only along the named axis.
4. **Spec extension:** add fingerprint for each mode (sym/symw/
   asym/asymw/zpd/sp0/sp1/ppd/per), each side ('b'/'l'/'r'), and
   the 2-D forms. `tol = 0`.

## Out of scope for this ТЗ

- 3-D `wextend` (type=3) — not in 2025b help.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Substantial closure — closed all 3 gaps.

  **1. Extended modes** (gap #1): added 'symw' (whole-point symmetric),
  'asym' / 'asymh' (antisymmetric half-point), 'asymw' (antisymmetric
  whole-point), 'sp0' (replicate edge = order-0 spline), 'sp1' (linear
  extrapolation = order-1 spline). Now 11 modes total.

  **2. type=2** (gap #2): 2-D matrix extension. Refactored 1-D loop into
  `extend1D` helper; 2-D path applies it along columns then rows.

  **3. type='ar'/'ac'** (gap #3): along-row / along-col forms.
  Counter-intuitive MATLAB convention: 'ar' (along row direction) ADDS
  ROWS (extends columns); 'ac' (along col direction) ADDS COLS. Got
  this swapped first time around — caught by parity probe.

  Spec extended from 12 to 38 fingerprints (all 11 modes + 4 type
  forms + sides). Parity OK numkit ↔ MATLAB at tol=0. Octave doesn't
  ship `wextend` (Wavelet Toolbox). 14 TEST_F gtest (existing 6 + 8
  new). 113+ wavelet-suite tests still pass — no regression.
