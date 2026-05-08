# wavelet/wrev — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 1c2df89
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/dwt/wkeep_wextend.cpp` (`wrev`)
- Spec: `tools/parity/specs/wrev.json`
- `y = wrev(x)` — vector reverse (matches MATLAB)
- Behaviour on 2-D matrix: needs verification (probe interrupted
  before this case ran)

## MATLAB R2025b — actual behavior

`y = wrev(x)`:
- Vector input → reverses element order (same as `flip(x)`)
- Matrix input → reverses each **column** independently (i.e.
  `flipud(x)`). Probe: `wrev([1 2 3; 4 5 6]) = [4 5 6; 1 2 3]`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | 2-D matrix input | flipud (per-column reverse) | needs probe (probe was interrupted by upstream coif2 error) | unknown |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `wrev([1 2 3 4 5])` (row) | `[5 4 3 2 1]` | identical ✅ |
| `wrev([1; 2; 3; 4; 5])` (col) | `[5; 4; 3; 2; 1]` | identical ✅ |
| `wrev([1 2 3; 4 5 6])` | `[4 5 6; 1 2 3]` | needs probe |

## Recommended fixes

1. **Verify 2-D behaviour:** if numkit returns the same result as
   `flipud` (per-column), fine. If it does row-reverse instead
   (returns `[3 2 1; 6 5 4]`), align with MATLAB.
2. **Spec extension** — add fingerprint for matrix input,
   complex input. `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Two real bugs caught by spec extension:

  **1. Matrix path was full-flip not flipud** — previous impl
  treated input as a flat numel-vector and reversed in column-
  major order. For 2×3 [1 2 3; 4 5 6] that gave [6 5 4; 3 2 1]
  (full reversal). MATLAB returns flipud → [4 5 6; 1 2 3] (each
  column reversed independently).

  **2. Complex input dropped imaginary parts** — used
  `elemAsDouble(...) -> doubleDataMut[]`, which silently coerces
  to real. Now branches on `isComplex()` and uses
  `complexData()/complexDataMut()`.

  Rewrote with two paths (row-vec linear reverse; matrix per-
  column flipud), each having a real and complex branch.

  Spec extended from 5 to 13 fingerprints (row + col + matrix +
  complex + single-element). Parity OK numkit ↔ MATLAB at tol=0.
  Octave's wrev rejects matrices (its own limitation); we follow
  MATLAB. 8 TEST_F gtest (existing 6 + 2 new MatrixIsFlipud /
  ComplexPreservesImaginary). 22 wavelet-suite tests still pass —
  no regression.
