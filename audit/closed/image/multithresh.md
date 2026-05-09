# image/multithresh — ТЗ for completion

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
- Closed in commit: pending (multithresh fix)
- Closed date: 2026-05-09
- Notes: Initial closure (cycle 44) was DEFERRED -- numkit returned histogram-bin BOUNDARIES (0..L-1 indices) divided by L-1, MATLAB returns MIDPOINTS of adjacent class MEANS in the input's native value range (uint8 -> 0..255 floored, double -> 0..1). Fix: post-process Otsu best[] indices to compute class means then take midpoints; output range mirrors input class. Bit-identical with MATLAB R2025b on bimodal cluster probe (uint8 [20,120,220] -> [70, 170] both engines).
