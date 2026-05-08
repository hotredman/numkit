# builtin/interp3 — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 3cb06a1
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK`.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Misc batch 4 (convert + intmax/intmin + collection + meshgrid + misc, 20 funcs).
  Bit-identical MATLAB R2025b. See misc4_batch_test.cpp.
  KNOWN GAP: numkit's interp3 adapter has incorrect arg-shape validation (rejects valid grids). Documented as separate ТЗ.
