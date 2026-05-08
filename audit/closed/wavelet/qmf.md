# wavelet/qmf — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 1c2df89
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/filter/qmf.cpp` (`qmf`)
- Spec: `tools/parity/specs/qmf.json`
- `y = qmf(x[, p])` — `p=0` default

## MATLAB R2025b — actual behavior

`y = qmf(x[, p])`. `y(k) = (-1)^(k-1+p) · x(N-k+1)`. Default `p=0`.

## Gaps (numkit vs MATLAB)

**No major gap detected.** Probed values match.

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `qmf([1 2 3 4])` (default p=0) | `[4 -3 2 -1]` | `[4 -3 2 -1]` ✅ |
| `qmf([1 2 3 4], 1)` | `[-4 3 -2 1]` | `[-4 3 -2 1]` ✅ |

## Recommended fixes

1. **Spec extension** — add odd-length input, longer vectors.
   `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. Numkit qmf already
  matched MATLAB exactly across odd/even lengths, default and
  p=1 modes, column input, and single-element.

  Spec extended from 7 to 17 fingerprints (length-4 + length-5 +
  length-8 + p=0/p=1 + column + scalar). Parity OK numkit ↔
  MATLAB at tol=0 (integer-stable). Octave doesn't ship qmf
  (Wavelet Toolbox function); we follow MATLAB. 9 TEST_F gtest
  (existing 7 + 2 new EvenLength8 / Length4DefaultAndP1).
