# signal/blackman — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with the rest of signal.windows)
**Audited at commit:** 0e043c5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp:79` (`blackman`)
- Adapter: `libs/signal/src/windows/windows.cpp:496` (`blackman_reg`)
- Spec: `tools/parity/specs/blackman.json`
- 2nd arg silently ignored.

## MATLAB R2025b — actual behavior

Same surface as `hann` / `hamming`:

- `w = blackman(L)` / `(L, sflag)` / `(___, typeName)`

Symmetric: `0.42 − 0.5·cos(2π·n/(L−1)) + 0.08·cos(4π·n/(L−1))`.
Periodic: replace `(L−1)` with `L`.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `blackman(N, 'periodic')` silently returns symmetric | **high** |
| 2 | `'single'` typeName ignored | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `blackman(8)` | `[0 0.0905 0.4592 0.9204 0.9204 0.4592 0.0905 0]` | identical (modulo `-0` on endpoints) ✅ |
| `blackman(8, 'periodic')` | `[0 0.0664 0.34 0.7736 1 0.7736 0.34 0.0664]` | (returns symmetric) ❌ |

## Recommended fixes

Joint with `audit/findings/stats/hann.md`. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A — joint fix.
