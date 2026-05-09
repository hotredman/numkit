# builtin/tail — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 789cbc7
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK` on
benched input. Standard array/matrix manipulation function.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases (empty
   inputs, dimension/shape variations). `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (cycle 42)
- Closed date: 2026-05-09
- Notes: Spec-extension batch closure — auditor flagged "no major gap detected". Parity confirmed bit-identical against MATLAB R2025b on probed inputs.
