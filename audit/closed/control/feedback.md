# control/feedback — ТЗ for completion

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
- Notes: Initial closure was DEFERRED. Re-probed: feedback(tf, tf) returns a struct that differs slightly from MATLAB tf-object (numkit does NOT zero-pad numerator to denominator length, MATLAB does -- same H(s) semantically). Denominator is bit-identical. Spec uses tfdata(sys, "v") to extract canonical (num, den) vectors; sum(den) matches MATLAB exactly.
