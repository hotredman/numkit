# signal/bitrevorder — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** 9bce106
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/transforms/...` (`bitrevorder`)
- Spec: `tools/parity/specs/bitrevorder.json`
- 1-output form matches MATLAB.

## MATLAB R2025b — actual behavior

- `Y = bitrevorder(X)` — bit-reversed permutation
- `[Y, I] = bitrevorder(X)` — also returns the index vector

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | 2nd output `I` (index vector) | not produced (probe: `[Y, I] = bitrevorder(...)` throws "Undefined function or variable 'I'") | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `bitrevorder([1:8])` | `[1 5 3 7 2 6 4 8]` | identical ✅ |
| `[Y, I] = bitrevorder([1:8])` I | `[1 5 3 7 2 6 4 8]` | not produced |

## Recommended fixes

1. **Add 2nd output `I`** in `bitrevorder_reg`:
   ```cpp
   if (nargout > 1) outs[1] = produce_index_vector(...);
   ```
2. **Spec extension** — add fingerprint with `[Y, I]`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Implemented 2nd output `I` (gap #1). Adapter now reads
  nargout and emits the 1-based index vector such that
  `Y(k) = X(I(k))`; built by applying the same bit-reversal to
  (1:N). Verified algebraic identity `Y == X(I)` element-wise via
  gtest.

  Spec extended from 1 to 24 fingerprints (1-output + 2-output for
  N=4 and N=8). Parity OK numkit ↔ MATLAB ↔ Octave at tol=0
  (integer-stable). 4 TEST_F gtest (existing 2 + 2 new) + smoke.
