# stats.descriptive/corrmtx — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 015c30d
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK`.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (re-probe)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED. Re-probed: corrmtx(x, p) returns size-(N-p+p+1)x(p+1) data matrix bit-identical with MATLAB R2025b (size 7x3 on probed input). Earlier defer was wrong; spec restored.
