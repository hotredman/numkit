# builtin/rdivide — ТЗ for completion

**Status:** open
**Priority:** medium (PROGRESS notes `correctness=MISMATCH` on bench)
**Effort:** small
**Audited at commit:** 42e1ec3
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | Probed basic cases (`rdivide(-1, 3) = -0.333`, matrix forms) match MATLAB ✓ but PROGRESS bench (1M-pt div) flags MISMATCH — needs targeted re-probe (likely division-by-zero handling: 1/0 = Inf or NaN convention). | medium |

## Recommended fixes

1. **Probe edge cases**: 1/0 → Inf, 0/0 → NaN, -1/0 → -Inf,
   1/-0 → -Inf etc. Verify against MATLAB's IEEE-754 semantics.
2. **Spec extension** after divergence is identified.

## Out of scope for this ТЗ

- N/A.
