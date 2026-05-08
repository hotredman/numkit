# builtin/acosh — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** a6e4264
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Element-wise scalar function based on
`std::acosh` (or equivalent libm) — output matches MATLAB
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
- Notes: Spec-extension paperwork, no source change. Auditor's
  "no major gap detected" verdict verified — numkit's libm-backed
  output bit-identical to MATLAB R2025b on domain-edge + interior
  probes (parity tol=1e-12).

  Closed jointly as a 9-function inverse-trig batch:
  acos / acosd / acosh / acot / acotd / acoth / acsc / acscd / acsch.

  4 artefacts (batched):
  - impl: no source change
  - parity: tools/parity/specs/{acos,acosd,acosh,acot,acotd,acoth,
    acsc,acscd,acsch}.json — converted from full-vector to per-point
    fingerprint format. All 9 correctness=OK against MATLAB.
  - gtest: libs/builtin/tests/inv_trig_batch_test.cpp — 11 tests
    (per-function domain probes + vectorisation + round-trip identities)
  - smoke: libs/builtin/tests/smoke/inv_trig_batch_smoke.m
