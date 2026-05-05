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
