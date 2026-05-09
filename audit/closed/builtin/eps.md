# builtin/eps — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 7a3e258
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK`.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge inputs and type
   conversions. `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Math+reductions spec-extension batch (11 funcs). All
  bit-identical MATLAB R2025b. See math_reductions_batch_test.cpp.
  KNOWN GAPS (separate ТЗ — surfaced during this batch but
  out of scope): eps() with no args returns empty (should return
  eps(1)); eps(0.5) confused as indexing; eps(vector) segfaults.
  Only the working scalar-positive path is pinned.
