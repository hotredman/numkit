# wavelet/wmaxlev — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 0e895fe
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/dwt/dyad.cpp:118` (`wmaxlev_reg`)
- Spec: `tools/parity/specs/wmaxlev.json`
- What works today:
  - `L = wmaxlev(N, wname)` — `floor(log2(N / (Lf - 1)))`
  - Vector `N` — uses `min(N)` per MATLAB

## MATLAB R2025b — actual behavior

Documented signatures (`help wmaxlev`):

- `L = wmaxlev(S, wname)` — S is signal length (scalar) or 2-vector
  for 2-D image dimensions

## Gaps (numkit vs MATLAB)

**No major behavioural gap detected.**

| # | Coverage gap | Recommendation |
|---|---|---|
| 1 | Spec covers single case | extend |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `wmaxlev(100, 'db4')` | `3` | `3` ✅ |
| `wmaxlev([50 100], 'db4')` | `2` | `2` ✅ |
| `wmaxlev(1024, 'haar')` | `10` | `10` ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint over (wavelet ∈ {haar, db4,
   coif2, sym4}) × (N ∈ {16, 100, 1024}) plus the 2-vector form.
   `tol = 0`.
2. **PROGRESS.md row update:** unchanged — comment is accurate.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. Numkit wmaxlev
  already matched MATLAB exactly across all probed cases.

  Spec extended from 4 to 12 fingerprints across all documented
  wavelet families (haar, db1, db2, db4, db10, sym4, coif2) ×
  N ∈ {2, 8, 16, 64, 100, 1024, 2048} + 2-vector N (image-dim
  form). Parity OK numkit ↔ MATLAB at tol=0. Octave doesn't ship
  `wmaxlev` (Wavelet Toolbox); we follow MATLAB. 11 TEST_F gtest
  (existing 5 + 6 new AllFamiliesAtN100 / LargerWavelet /
  ImageDimsTakeMin / BoundaryShortSignal / LargeHaarPowerOfTwo).
