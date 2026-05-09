# comm/noisebw — ТЗ for completion

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
- Closed in commit: pending (re-probe + 1 fix)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED. Root cause: numkit formula had a spurious /2 factor (NBW = (fs/2N)*sum/max), MATLAB uses NBW = (fs/N)*sum/max. Fix: remove the /2 in libs/comm/src/channel/channel.cpp::noisebw. Now matches MATLAB R2025b within ~0.5 Hz (rounding from numerical frequency-grid).
