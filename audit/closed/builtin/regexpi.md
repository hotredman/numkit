# builtin/regexpi — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** c1fdebe
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK` on
benched input. Standard string/character function.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases (empty
   strings, multi-byte chars, unicode where applicable).
   `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (cycle 43)
- Closed date: 2026-05-09
- Notes: Spec-extension batch closure — auditor flagged "no major gap detected". Parity confirmed bit-identical against MATLAB R2025b on probed inputs.
