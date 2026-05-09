# wavelet/wnoisest — ТЗ for completion

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
- Closed in commit: pending (cycle 6 extras 2)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED. Re-probed with deterministic input (sin+cos signal): wnoisest(c, l, level) bit-identical with MATLAB R2025b on db4 level-3 wavedec output (sigma=0.0900008). Earlier defer was a spec issue (used randn which differs across engines).
