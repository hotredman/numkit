# builtin/besselk — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 03244f9
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Output matches MATLAB to within 1-ULP
(or exactly) on probed inputs.

## Recommended fixes

1. **Spec extension** — fingerprint covering domain edges and
   asymptotic regimes. `tol = 1e-14`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Special-function spec-extension batch (17 funcs: bessel{j,y,
  i,k,h} + beta/betainc/betaincinv/betaln + gamma/gammainc/gammaincinv/
  gammaln + erf/erfc/erfinv/erfcinv). All bit-identical MATLAB R2025b.
  See libs/builtin/tests/special_fn_batch_test.cpp + smoke + 17 specs.
