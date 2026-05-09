# optim/optimget — ТЗ for completion

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
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Multi-namespace batch (io+linalg+wavelet+optim, 21 funcs).
  Bit-identical MATLAB R2025b on probed inputs.
  KNOWN GAP: optimget struct field-access differs. Documented as separate ТЗ.
