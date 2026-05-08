# builtin/lcm — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** f82f380
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Output matches MATLAB bit-for-bit on
probed inputs.

## Recommended fixes

1. **Spec extension** — fingerprint covering domain edges + complex
   inputs (where applicable). `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Cell+struct + number-theory batch (18 funcs). All bit-identical
  MATLAB R2025b. See cell_struct_numtheory_batch_test.cpp.
