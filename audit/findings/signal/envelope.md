# signal/envelope — ТЗ for completion

**Status:** open
**Priority:** **critical** (PROGRESS already MISMATCH)
**Effort:** medium
**Audited at commit:** 9bce106
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/transforms/...` (`envelope`)
- Spec: `tools/parity/specs/envelope.json` (PROGRESS:
  `correctness=MISMATCH`)
- What works today: returns 1 output (upper envelope only)

## MATLAB R2025b — actual behavior

- `[yupper, ylower] = envelope(x)` — analytic-signal envelope
- `envelope(x, fl)` — windowed Hilbert with filter length
- `envelope(x, np, 'analytic')` / `'rms'` / `'peak'` — multiple
  modes

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | 2nd output `ylower` | populated | numkit returns empty for 2nd out (probe: `lo = empty`) | **high** |
| 2 | numeric values | analytic envelope | diverge (PROGRESS MISMATCH); root cause likely the upstream `hilbert` sign flip + different filter-length default | high |
| 3 | `envelope(x, fl)` filter-length form | windowed | not supported | medium |
| 4 | `'rms'` / `'peak'` modes | non-analytic envelopes | not supported | medium |

## Reference table (from probe)

Inputs: `sig = sin(2π·0.1·(0:31)) · exp(-0.05·(0:31))`

| Inputs | MATLAB | numkit |
|---|---|---|
| `[up, lo] = envelope(sig)` up head | `[0.479 0.826 0.865 0.899 0.930 0.882]` | `[0.437 0.812 0.865 0.895 0.910 0.840]` ❌ |
| `lo` head | `[-0.397 -0.744 -0.783 -0.817 -0.848 -0.800]` | empty (only 1 out) |

## Recommended fixes

1. **Populate 2nd output `ylower`:** `ylower = -yupper` for the
   analytic-signal path is the symmetric pair (both wrap a real
   symmetric envelope). If numkit's adapter only writes `outs[0]`,
   add `if (nargout > 1) outs[1] = ...`.
2. **Fix the cascade from `hilbert`:** once
   `audit/findings/signal/hilbert.md` lands, re-probe envelope
   numerics — the sign flip in hilbert directly corrupts the
   analytic-signal magnitude calculation.
3. **Implement filter-length form `envelope(x, fl)`:** windowed
   Hilbert with FIR filter design.
4. **Implement `'rms'` and `'peak'` modes.**
5. **Spec extension:** PROGRESS notes MISMATCH; regenerate after
   the cascade fix. `tol = 1e-9`.

## Out of scope for this ТЗ

- N-D envelope.
