# signal/dftmtx — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 9bce106
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/transforms/...` (`dftmtx`)
- Spec: `tools/parity/specs/dftmtx.json`
- `F = dftmtx(N)` — produces N×N DFT matrix; matches MATLAB exactly.

## MATLAB R2025b — actual behavior

`F = dftmtx(N)` — only signature.

## Gaps (numkit vs MATLAB)

**No major gap detected.**

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `dftmtx(4)` F(2,2) | `0 - 1i` | `~0 - 1i` ✅ (within rounding) |
| `dftmtx(4)` F(2,3) | `-1 + 0i` | `-1 + ~0i` ✅ |

## Recommended fixes

1. **Spec extension** — N values {2, 4, 8, 16, 32} fingerprint.
   `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec coverage, no impl change. Numkit dftmtx already
  matched MATLAB exactly modulo IEEE rounding noise on cells that
  are algebraically zero (sub-1e-12). Spec extended from 3 to 9
  fingerprints (N ∈ {1, 2, 4, 8, 16} sizes + algebraic identities
  at off-diagonal cells). Parity OK numkit ↔ MATLAB ↔ Octave at
  tol=1e-12. 6 TEST_F gtest (existing 3 + 3 new for N=1, N=2,
  N=16) + smoke.
