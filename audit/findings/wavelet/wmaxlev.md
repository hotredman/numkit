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
