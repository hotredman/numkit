# comm/rayleighchan — ТЗ for completion

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
- Closed in commit: pending (trivial-fix batch)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED. Re-classified N/A: MATLAB R2025b DEPRECATED rayleighchan in favour of comm.RayleighChannel system object -- the function-form rayleighchan no longer exists at the MATLAB top level. Numkit ships rayleighchan as a convenience helper (returns one complex Rayleigh-distributed sample). No MATLAB reference to compare against.
