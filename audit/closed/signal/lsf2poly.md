# signal/lsf2poly — ТЗ for completion

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
- Closed in commit: pending (lsf2poly fix)
- Closed date: 2026-05-09
- Notes: Initial closure (cycle 40) was DEFERRED -- output length off-by-one. Root cause: numkit always assigned (1+z^-1) factor to P and (1-z^-1) to Q, leaving P and Q at mismatched degrees so the trailing-zero cancellation broke. Fix: distribute boundary roots by parity (m odd: Q gets (1-z^-2); m even: P gets (1+z^-1), Q gets (1-z^-1)) so both polynomials have degree m+1, then drop the final element. Verified bit-identical with MATLAB R2025b on m=2,3,4,5 probes.
