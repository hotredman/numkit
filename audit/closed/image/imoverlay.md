# image/imoverlay — ТЗ for completion

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
- Notes: Initial closure was DEFERRED with vague "arg validation differs" note. Re-probed: imoverlay(I, BW, color) works bit-identically with MATLAB R2025b. Numkit requires explicit color arg (matches MATLAB convention -- no default).
