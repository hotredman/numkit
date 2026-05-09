# comm/gaussdesign — ТЗ for completion

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
- Closed in commit: pending (re-probe batch)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED with vague 'output dimensions/normalization differ' note. Re-probed: numkit gaussdesign(BT, span, sps) returns the same length (sps*span+1=33), same sum (1.0), and the same per-tap values as MATLAB R2025b (h(17)=0.112904 verified). Earlier defer was wrong; spec restored.
