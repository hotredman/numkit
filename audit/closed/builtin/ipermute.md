# builtin/ipermute — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 789cbc7
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK` on
benched input. Standard array/matrix manipulation function.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases (empty
   inputs, dimension/shape variations). `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Shape-ops spec-extension batch (16 funcs). All bit-identical
  MATLAB R2025b. See shape_batch_test.cpp + smoke + 16 specs.
