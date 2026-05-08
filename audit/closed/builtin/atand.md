# builtin/atand — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** a6e4264
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Element-wise scalar function based on
`std::atand` (or equivalent libm) — output matches MATLAB
bit-for-bit on probed inputs.

## Recommended fixes

1. **Spec extension** — fingerprint covering domain edges + complex
   inputs (where applicable). `tol = 0` for libm-bit-exact match,
   or `tol = 1e-15` for unit-rounding agreement.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Spec-extension paperwork batch 2 (sibling of inv_trig_batch).
  Closed jointly: asin/asind/asinh + atan/atand/atanh + asec/asecd/asech.
  All bit-identical to MATLAB R2025b. See libs/builtin/tests/
  inv_trig_batch2_test.cpp + smoke + parity specs.
