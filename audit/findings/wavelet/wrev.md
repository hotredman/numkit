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
