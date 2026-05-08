# wavelet/coifwavf — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** 1c2df89
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/filter/families.cpp` (`coifwavf`)
- Spec: `tools/parity/specs/coifwavf.json`
- Only `coif1` supported (probe shows `coif2` throws)

## MATLAB R2025b — actual behavior

- `h = coifwavf(wname)` — supports `'coif1'..'coif5'`

Length = 6K for `'coifK'`.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | only coif1 supported (vs MATLAB coif1..coif5) | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `coifwavf('coif1')` | `[-0.0514 0.2389 0.6029 0.2721 -0.0514 -0.0111]` | identical ✅ |
| `coifwavf('coif2')` head | `[0.0116 -0.0293 -0.0476 0.2730]` | THROWS |

## Recommended fixes

1. **Extend Coiflet coefficient table to coif5.** Public-domain
   tables exist; lengths are 6, 12, 18, 24, 30 for K = 1..5.
2. **Spec extension** — add fingerprint for coif2..coif5 once
   supported. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Extended Coiflet table from coif1 to coif1..coif5
  (full MATLAB family — coiflets only go up to coif5). Coefficients
  extracted from MATLAB R2025b `flip(coifwavf*sqrt(2))` at
  17-decimal precision.

  **Sum precision:** MATLAB's published Coiflet coefficients are
  decimal-truncated, so `sum(coifwavf('coif5'))` in MATLAB itself
  is 1.0000000001 (1e-10 error). Numkit reproduces the same
  truncated values and matches MATLAB exactly within that
  precision. The `ExtendedSumsToOne` gtest uses tol=1e-9 to
  reflect this inherent limitation.

  Spec extended from 5 to 17 fingerprints. Parity OK numkit ↔
  MATLAB at tol=1e-12. Octave doesn't ship `coifwavf`. 7 TEST_F
  gtest (existing 4 + 3 new Coif2ToCoif5Lengths /
  ExtendedSumsToOne / Coif2HighPrecision).
