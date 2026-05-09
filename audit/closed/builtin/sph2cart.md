# builtin/sph2cart — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** a6e4264
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Element-wise scalar function based on
`std::sph2cart` (or equivalent libm) — output matches MATLAB
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
- Notes: Misc batch 6 (rng/shape/typecast/strings, 11 funcs).
  Bit-identical MATLAB R2025b. See misc6_batch_test.cpp.
