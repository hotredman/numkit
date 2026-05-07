# stats.dist/betastat — ТЗ for completion

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
| `betastat(2, 3)` | `m=0.4, v=0.04` | identical ✅ |

## Recommended fixes

1. **Spec extension** — fingerprint over (a, b) variations.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: **Found a real gap during spec extension** (audit said
  "no major gap" — but ТЗ-probe was scalar-only). Numkit's
  `betastat_reg` was scalar-only (`args[0].toScalar()`); MATLAB
  vectorises element-wise with broadcasting. Fixed:
  - betastat_reg now does MATLAB-style broadcasting: equal sizes OR
    one scalar; otherwise dim-mismatch error. Scalar fast path
    preserved.
  - Spec covers vector + scalar+vector + invalid (a≤0).
  - 5 TEST_F gtest + smoke .m. Parity OK numkit ↔ MATLAB ↔ Octave.
