# wavelet/dyadup — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small (joint with `dyaddown`)
**Audited at commit:** 0e895fe
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/dwt/dyad.cpp:80` (`dyadup_reg`)
- Spec: `tools/parity/specs/dyadup.json`
- What works today:
  - `Y = dyadup(X[, ODD])` — vector input, ODD=1 default
    (zero insertion gives length 2N+1)

## MATLAB R2025b — actual behavior

Same surface as `dyaddown`:

- `Y = dyadup(X)` / `(X, EVENODD)` / `(___, 'type')`

`'type'` ∈ `'c'` / `'r'` / `'m'`.

## Gaps (numkit vs MATLAB)

Same as `dyaddown` (see `audit/findings/wavelet/dyaddown.md`):
- `'type'` 3rd arg — likely throws or ignored
- matrix input default behaviour — needs verification

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `dyadup([1:8]')` (default ODD=1) | length 17 zero-interleaved | matches ✅ |
| `dyadup([1:8]', 0)` | length 15 | matches ✅ |
| `dyadup(M, 1, 'c')` 2-D | column-wise | needs probe — adapter likely throws |

## Recommended fixes

Joint with `audit/findings/wavelet/dyaddown.md`. Same adapter
shape; the 'type' parser is identical.

Spec extension: same pattern (vector / matrix × ODD={0,1} ×
type). `tol = 0`.

## Out of scope for this ТЗ

- N/A — joint fix.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Joint closure with audit/closed/wavelet/dyaddown.md.
  Same fix shape: matrix path now applies 'c' / 'r' / 'm'
  upsampling (zero-insertion) instead of silently flattening.
  Spec extended from 7 to 31 fingerprints. Parity OK numkit ↔
  MATLAB at tol=0. Octave doesn't ship dyadup. 9 TEST_F gtest
  (existing 6 + 3 new MatrixDefaultColumnUpsample / MatrixTypeR /
  MatrixTypeM).
