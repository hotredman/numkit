# signal/hamming — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with `hann` and the rest of signal.windows)
**Audited at commit:** 0e043c5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp:53` (`hamming`)
- Adapter: `libs/signal/src/windows/windows.cpp:480` (`hamming_reg`)
- Spec: `tools/parity/specs/hamming.json`
- 2nd arg silently ignored (same as `hann`).

## MATLAB R2025b — actual behavior

Same surface as `hann`:

- `w = hamming(L)` / `(L, sflag)` / `(___, typeName)`

Symmetric: `0.54 − 0.46·cos(2π·n / (L−1))`. Periodic: same with
`(L−1)` ⇒ `L`.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `hamming(N, 'periodic')` silently returns symmetric | **high** |
| 2 | `'single'` typeName ignored | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `hamming(8)` | `[0.08 0.2532 0.6424 0.9544 0.9544 0.6424 0.2532 0.08]` | identical ✅ |
| `hamming(8, 'periodic')` | `[0.08 0.2147 0.54 0.8653 1 0.8653 0.54 0.2147]` | (returns symmetric — wrong) ❌ |

## Recommended fixes

Joint with `audit/findings/stats/hann.md` — same adapter rewrite
shape; just bind the Hamming formula. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A — joint fix.
