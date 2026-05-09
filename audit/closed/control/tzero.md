# control/tzero — ТЗ for completion

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
- Closed in commit: pending (cycle 7)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED -- numkit zero(sys) on SS form threw "not yet implemented". Fix in libs/control/src/props/props.cpp::zerosOf: SISO state-space falls through ss2tf (already implemented) + roots(num) to compute transmission zeros. Bit-identical with MATLAB R2025b on probed SISO system A=[0 1;-2 -3], B=[0;1], C=[-1 1], D=0 -> z = 1.0. MIMO case still requires the generalised eigenvalue (QZ) solver -- now throws an explicit error pointing to that as a separate ТЗ.
