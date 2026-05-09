# builtin/mldivide — ТЗ for completion

**Status:** closed
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

## Closed
- Closed in commit: see commit 6b43f00a (joint with mrdivide closure)
- Closed date: 2026-05-08
- Notes: Implemented as part of the mrdivide ТЗ batch. Both square
  (LU with partial pivoting) and tall (QR via Householder + R back-
  solve, least-squares) paths land via the shared private helper
  numkit::builtin::detail::la_solve(). Wide A (m<n, min-norm) is
  explicitly NOT implemented — throws m:mldivide:wide. See
  audit/closed/builtin/mrdivide.md for the full implementation
  summary. Verification covered in libs/builtin/tests/mldivide_test.cpp
  (12 mldivide tests + 6 mrdivide tests, all bit-identical to
  MATLAB R2025b on tol=1e-10 across the parity-spec fingerprints).
