# signal/impzlength — ТЗ for completion

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
- Closed in commit: pending (impzlength fix)
- Closed date: 2026-05-09
- Notes: Initial closure (cycle 43) was DEFERRED -- min length 50 cap forced output to 50 even for fast-decaying filters. Fix: use MATLAB formula floor(log(5e-5) / log(rho)) directly, no minimum cap (other than the trivial 1). Bit-identical with MATLAB R2025b on rho = 0.1, 0.5, 0.7, 0.9, 0.99 probes (returns 4, 14, 27, 93, 985 -- all matching).
