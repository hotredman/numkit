# image/bwconncomp — ТЗ for completion

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
- Closed in commit: pending (struct outputs)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED with vague "object with field access syntax differs" note. Rewrote bwconncomp to return a 1x1 struct with fields {Connectivity, ImageSize, NumObjects, PixelIdxList} matching MATLAB R2025b exactly. PixelIdxList is now a 1xK cell of column-vector 1-based linear indices (was a NaN-padded matrix). Bit-identical with MATLAB on eye(5)>0 probe (Connectivity=8, ImageSize=[5 5], NumObjects=1, PixelIdxList{1}=[1;7;13;19;25]).
