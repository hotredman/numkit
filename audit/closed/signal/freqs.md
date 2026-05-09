# signal/freqs — ТЗ for completion

**Status:** closed
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
- Closed in commit: pending (trivial-fix batch)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED -- numkit returned 10x1 column vector, MATLAB returns 1x10 row vector for scalar w input. Fix: switched Value::complexMatrix(M, 1) -> Value::complexMatrix(1, M) in libs/signal/src/filter_design/analog_filters.cpp::freqs. Values were already bit-identical -- only the shape was wrong.
