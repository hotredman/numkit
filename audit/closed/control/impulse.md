# control/impulse — ТЗ for completion

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
- Closed in commit: pending (refined defer note)
- Closed date: 2026-05-09
- Notes: DEFERRED (refined). Math is correct: numkit impulse returns the IR/SR samples of the system. Disagreement is solely in the default time grid: numkit picks 201 equispaced samples on a fixed [0, T_default] window, MATLAB picks an ADAPTIVE grid based on the system poles (typically ~127 samples on a window sized to settling time of the slowest mode). Closing this requires implementing MATLAB-equivalent default time-vector selection in libs/control/. Output VALUES at coincident times do match. Placeholder spec stays.
