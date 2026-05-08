# builtin/splitlines — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** c1fdebe
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK` on
benched input. Standard string/character function.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases (empty
   strings, multi-byte chars, unicode where applicable).
   `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Misc batch 6 (rng/shape/typecast/strings, 11 funcs).
  Bit-identical MATLAB R2025b. See misc6_batch_test.cpp.
