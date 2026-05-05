# core/ifftshift — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** small (joint with `fftshift`)
**Audited at commit:** 9bce106
**Audit date:** 2026-05-06

## Текущая реализация

- Source: same module as `fftshift`
- Spec: `tools/parity/specs/ifftshift.json`
- Even-N matches MATLAB; odd-N is **swapped with `fftshift`**.

## MATLAB R2025b — actual behavior

`Y = ifftshift(X[, dim])`. Even N: same as `fftshift`. Odd N:
shifts the **first `floor(N/2)`** elements to the back (so
`fftshift` and `ifftshift` are inverses).

For `X = [1:7]`: `ifftshift = [4 5 6 7 1 2 3]` (move first 3 to back).

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `ifftshift([1:7])` (odd N) | `[4 5 6 7 1 2 3]` | `[5 6 7 1 2 3 4]` ❌ (= MATLAB's `fftshift`) |  **critical** |
| 2 | matrix and dim arg | as `fftshift` (with floor split) | likely same swap | critical |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `ifftshift([1:8])` (even) | `[5 6 7 8 1 2 3 4]` | identical ✅ |
| `ifftshift([1:7])` (odd) | `[4 5 6 7 1 2 3]` | `[5 6 7 1 2 3 4]` ❌ |

## Recommended fixes

Joint with `audit/findings/signal/fftshift.md`. The odd-N split
should be `floor(N/2)` for `ifftshift` and `ceil(N/2)` for
`fftshift`. Numkit currently has these reversed; one swap fixes
both.

Spec: same shape — odd-N coverage. `tol = 0`.

## Out of scope for this ТЗ

- N/A — joint fix.
