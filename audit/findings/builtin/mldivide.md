# builtin/mldivide — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** medium (joint with `mrdivide`)
**Audited at commit:** 42e1ec3
**Audit date:** 2026-05-06

## Gaps

PROGRESS notes `correctness=N/A` — possibly partially implemented
or unverified. Probe was interrupted by `mrdivide` THROW; need
focused re-probe.

## Recommended fixes

1. **Verify implementation:** test square (LU), tall (QR), wide
   (least-squares), rectangular, singular cases.
2. **Spec extension** after status confirmed.
3. Joint with `audit/findings/builtin/mrdivide.md` — both should
   land together.

## Out of scope for this ТЗ

- N/A.
