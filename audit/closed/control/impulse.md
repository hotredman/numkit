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
- Closed in commit: pending (cycle 3-4)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED -- numkit returned 201 samples vs MATLAB 127 for tf(1,[1 1]). Fix: changed default time grid in libs/control/src/response/response.cpp::pickGrid to MATLAB convention: Tfinal = -log(0.003)/min|Re(p)| ~= 5.80/min|Re(p)|, dt = Tfinal/126, N clamped to [60, 1000]. Bit-identical with MATLAB on 1st-order tf(1, [tau 1]) for tau in {0.1, 0.5, 1, 2, 5} (always 127 samples). Higher-order systems: numkit picks similar but not identical N (depends on max-pole speed).
