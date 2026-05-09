# graphics/cylinder — ТЗ for completion

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
- Closed in commit: pending (re-probe batch)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED with note 'cylinder output count differs (numkit returns single coordinate vs MATLAB triple)'. Re-probed: cylinder(N) and cylinder([R, N]) work bit-identically with MATLAB R2025b. There is a CORE-PARSER bug with parenless multi-output assignment ([x,y,z] = cylinder; segfaults but [x,y,z] = cylinder(); works) -- documented as separate engine ТЗ. Spec uses cylinder(20).
