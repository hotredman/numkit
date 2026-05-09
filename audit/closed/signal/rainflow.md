# signal/rainflow — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 4fae461
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK` on
benched input.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (rainflow output shape fix)
- Closed date: 2026-05-09
- Notes: Initial closure (cycle 43) was DEFERRED -- numkit returned Nx3 matrix [count, range, mean]; MATLAB returns Nx5 with two extra columns [start_idx, end_idx] giving the ORIGINAL signal indices (1-based) of each cycle's start and end turning points. Fix: track turning-point indices through the ASTM E1049-85 four-point algorithm and emit them as columns 4-5. The actual cycle-counting algorithm was already correct -- bit-identical with MATLAB on the canonical 9-sample probe x = [-2 1 -3 5 -1 3 -4 4 -2].
