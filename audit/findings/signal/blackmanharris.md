# signal/blackmanharris — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with the rest of signal.windows)
**Audited at commit:** 0e043c5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp:416` (`blackmanharris`)
- Adapter: `libs/signal/src/windows/windows.cpp:610` (`blackmanharris_reg`)
- Spec: `tools/parity/specs/blackmanharris.json`
- 2nd arg silently ignored.

## MATLAB R2025b — actual behavior

Same surface as `hann`:

- `w = blackmanharris(L)` / `(L, sflag)` / `(___, typeName)`

4-term Blackman-Harris coefficients [0.35875, 0.48829, 0.14128,
0.01168]. Symmetric uses `(L-1)` denominator; periodic uses `L`.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `blackmanharris(N, 'periodic')` silently returns symmetric | **high** |
| 2 | `'single'` typeName ignored | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `blackmanharris(8)` | `[6.0e-5 0.0334 0.3328 0.8894 0.8894 0.3328 0.0334 6.0e-5]` | identical ✅ |
| `blackmanharris(8, 'periodic')` | `[6.0e-5 0.0217 0.2175 0.6958 1 0.6958 0.2175 0.0217]` | (returns symmetric) ❌ |

## Recommended fixes

Joint with `audit/findings/stats/hann.md`. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A — joint fix.
