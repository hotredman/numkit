# image/col2im — ТЗ for completion

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
- Closed in commit: pending (cycle 3-4)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED with vague "arg-shape validation differs" note. Re-probed: col2im(B, [m n], [mm nn], type) works bit-identically with MATLAB R2025b on B = reshape(1:36, 4, 9), [m n]=[2 2], [mm nn]=[6 6], distinct mode (first row = [1 3 13 15 25 27], last row = [10 12 22 24 34 36]). Earlier defer was a spec issue (wrong B shape).
