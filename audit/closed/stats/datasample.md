# stats.descriptive/datasample — ТЗ for completion

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
- Closed in commit: pending (trivial-fix batch)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED -- numkit defaulted dim=1 always, so a row vector input was sampled along its 1 row giving wrong output. Fix: auto-pick dim=2 for row vectors (matches MATLAB), keep dim=1 default otherwise. Output SHAPE bit-identical with MATLAB; values may differ due to RNG cascade across engines.
