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
- Closed in commit: pending (refined defer note)
- Closed date: 2026-05-09
- Notes: DEFERRED (refined). numkit tzero supports TF input but throws on SS input with explicit message: "zero(sys) on state-space form not yet implemented; convert via [num,den] = ss2tf(A,B,C,D) then roots(num)." MATLAB tzero accepts SS via generalized eigenvalue problem of the system pencil [A-zI B; C D]. Closing this requires implementing the QZ-based transmission-zero solver in libs/control/. Workaround documented in error message.
