# optim/fminsearch — ТЗ for completion

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
- Closed in commit: pending (cycle 6 extras)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED. Re-probed: fminsearch(fn, x0) Nelder-Mead converges to MATLAB R2025b solution within tol on probed quadratic. Numkit returns ONLY x; multi-output form is a separate ТЗ.
