# signal/goertzel — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** d3d8da7
**Audit date:** 2026-05-06

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `goertzel(x)` 1-arg form | computes full DFT (default freq indices = 1..N) | adapter throws "goertzel: requires (x, ind)" | medium |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `goertzel(sig)` (1-arg) | 64×1 result (full DFT) | THROWS |
| `goertzel(sig, [5 15])` | 2-element result at requested bins | likely OK |

## Recommended fixes

1. **Default `ind = 1:N`** when only the signal is provided.
2. **Spec extension** — add fingerprint with default ind + explicit
   ind list.

## Out of scope for this ТЗ

- N/A.
