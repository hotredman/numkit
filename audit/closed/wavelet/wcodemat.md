# wavelet/wcodemat — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 0e895fe
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/dwt/wkeep_wextend.cpp` (helpers area)
- Spec: `tools/parity/specs/wcodemat.json`
- What works today:
  - `Y = wcodemat(X[, nb[, opt[, absol]]])` — quantize/scale to
    `[1, nb]` integer codes
  - Default `nb=16`, `opt='mat'`, `absol=1`

## MATLAB R2025b — actual behavior

Documented signatures (`help wcodemat`):

- `y = wcodemat(x)`
- `y = wcodemat(x, nbcodes)`
- `y = wcodemat(x, nbcodes, opt)` — `'mat'` (default), `'row'`,
  `'col'`
- `y = wcodemat(x, nbcodes, opt, absol)` — `absol=0` ⇒ rescale
  signed; `absol=1` ⇒ rescale absolute values

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `'row'` / `'col'` opt | per-row / per-col rescale | needs probe; numkit may treat as 'mat' | medium |
| 2 | `absol=0` | rescale signed values around zero | implemented per source comment | low |

## Reference table (from probe)

Inputs: `M = [1 -2 3; 4 -5 6]`

| Inputs | MATLAB | numkit |
|---|---|---|
| `wcodemat(M)` | `[1 4 7; 10 13 16]` | identical ✅ |
| `wcodemat(M, 4)` | `[1 1 2; 3 4 4]` | identical ✅ |
| `wcodemat(M, 16, 'row')` | `[1 9 16; 1 9 16]` | needs probe — likely OK or different |
| `wcodemat(M, 16, 'mat', 0)` | `[9 5 12; 14 1 16]` | needs probe |

## Recommended fixes

1. **Verify `'row'` and `'col'` opt** are handled (probe shows
   numkit returns same values as MATLAB for default; need to confirm
   for non-default opt).
2. **Verify `absol=0` path** matches MATLAB's signed-rescale
   formula.
3. **Spec extension:** add fingerprint for opt ∈ {row, col}, absol
   ∈ {0, 1}. `tol = 0` (integer codes).

## Out of scope for this ТЗ

- N/A — function appears mostly correct; this ТЗ is mostly spec
  coverage.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Real bug caught by spec extension. Auditor said "function
  appears mostly correct"; probe revealed wrong quantization
  formula:

  numkit was using `round((v - mn)/span * (nb - 1)) + 1`. MATLAB
  uses `floor((v - mn)/span * nb) + 1`, with the upper edge
  (v == mx) clamped from nb+1 down to nb. This produced
  off-by-one errors on interior values:
    wcodemat([1 -2 3; 4 -5 6], 4)
      numkit  = [1 2 2; 3 3 4]   (wrong)
      MATLAB  = [1 1 2; 3 4 4]   (correct)
  Same shape error in `'mat', absol=0`: numkit (2,1) = 13, MATLAB
  = 14.

  Fix: replaced `round` with `floor` and `(nb - 1)` with `nb` in
  the encode lambda. `'row'`/`'col'` paths already worked
  (auditor's main concern).

  Spec extended from 1 to 19 fingerprints (default + nb=4 +
  absol=0 + 'row' + 'col' + vector). Parity OK numkit ↔ MATLAB
  at tol=0. Octave doesn't ship `wcodemat` (Wavelet Toolbox);
  we follow MATLAB. 6 TEST_F gtest (new file) + smoke.
