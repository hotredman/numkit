# stats.dist/expstat — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.**

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `expstat(2)` | `m=2, v=4` | identical ✅ |

## Recommended fixes

1. **Spec extension** — vector inputs.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: Same vectorisation gap as betastat / chi2stat — adapter
  was scalar-only; now does elementwise on vector / matrix mu.
  Scalar fast path preserved. Spec covers scalar (mu=2) + vector
  ([1 2 5 10]) + mu=0 + mu<0 → NaN. 3 TEST_F gtest + smoke .m.
  Parity OK numkit ↔ MATLAB ↔ Octave at tol=1e-12.
