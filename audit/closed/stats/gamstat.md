# stats.dist/gamstat — ТЗ for completion

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
| `gamstat(2, 1)` | `m=2, v=2` | identical ✅ |

## Recommended fixes

1. **Spec extension** — vector inputs.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: Vectorisation via emit_vec_stat_2arg (sweep 5dd32c38).
  Impl edges already correct (a<=0 / b<=0 → NaN). 14-fingerprint
  spec; 4 TEST_F gtest + smoke. Parity OK numkit ↔ MATLAB ↔ Octave.
