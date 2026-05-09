# image/graythresh — ТЗ for completion

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
- Closed in commit: pending (trivial-fix batch)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED -- numkit returned bin-boundary-based threshold via lvl/(L-1), MATLAB returns mean(find(sigma_b == max)) / (L - 1) (mean of all tied-maximum bin indices). Fix: track all tied bins, average their indices, then divide by L-1. Note multithresh uses a DIFFERENT MATLAB convention (midpoint of class means) -- intentional split in MATLAB API. Bit-identical with MATLAB R2025b on bimodal probe (uint8 [20,120,220] -> 0.4686).
