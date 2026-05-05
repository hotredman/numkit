# core/fftshift — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** small
**Audited at commit:** 9bce106
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/builtin/src/...` (`fftshift`) — promoted to core
  per PROGRESS
- Spec: `tools/parity/specs/fftshift.json`
- Even-N case matches MATLAB; odd-N case is **swapped with
  `ifftshift`**

## MATLAB R2025b — actual behavior

`Y = fftshift(X[, dim])`. Even N: swap halves. Odd N: shift the
**first `ceil(N/2)`** elements to the back.

For `X = [1:7]`: `fftshift = [5 6 7 1 2 3 4]` (move first 4 to back).

`fftshift` and `ifftshift` are inverses for any N — they differ
only at odd N.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `fftshift([1:7])` (odd N) | `[5 6 7 1 2 3 4]` (split at 4) | `[4 5 6 7 1 2 3]` (split at 3) — **= MATLAB's `ifftshift`** | **critical — fftshift and ifftshift are SWAPPED for odd N** |
| 2 | matrix shift | both axes shifted with proper split | numkit's matrix shift differs (probe: numkit returns `[5 6 4; 8 9 7; 3 1 2]` vs MATLAB `[9 7 8; 3 1 2; 6 4 5]`) | **critical** |
| 3 | `fftshift(M, 1)` dim arg | shift along dim only | numkit returns same as no-dim | high |

## Reference table (from probe)

Inputs:

| Inputs | MATLAB | numkit |
|---|---|---|
| `fftshift([1:8])` (even) | `[5 6 7 8 1 2 3 4]` | `[5 6 7 8 1 2 3 4]` ✅ |
| `fftshift([1:7])` (odd) | `[5 6 7 1 2 3 4]` | `[4 5 6 7 1 2 3]` ❌ |
| `fftshift([1 2 3; 4 5 6; 7 8 9])` | `[9 7 8; 3 1 2; 6 4 5]` | `[5 6 4; 8 9 7; 3 1 2]` ❌ |
| `fftshift(M, 1)` | `[7 8 9; 1 2 3; 4 5 6]` | same as no-dim ❌ |

## Recommended fixes

1. **Swap the odd-N split direction.** Current numkit splits at
   `floor(N/2)` for `fftshift`; MATLAB splits at `ceil(N/2)`.
   ```cpp
   const size_t split = (N + 1) / 2;   // ceil(N/2)
   // shift: dest[k] = src[(k + split) % N]
   ```
2. **Implement `dim` arg:** when `args[1]` is a numeric scalar,
   shift only along that dim.
3. **Joint fix with `ifftshift`** — they are siblings; one fix
   updates both (the `split` direction is the only difference).
4. **Spec extension:** add fingerprint for odd N (lengths 1, 3, 5,
   7, 11), matrix forms, and `dim` arg. `tol = 0`.

## Out of scope for this ТЗ

- N-D `fftshift` — the core algorithm is the same recursion.
