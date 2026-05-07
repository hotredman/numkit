# stats.dist/expinv — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | `expinv(p)` 1-arg form likely throws (similar to `exppdf`) | medium |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `expinv(0.5, 2)` | `1.3862943611` | identical ✅ |
| `expinv(0.95, 2)` | `5.9914645471` | identical ✅ |

## Recommended fixes

1. **Default `mu = 1`** when 1-arg.
2. **Spec extension** — fingerprint over default + non-default mu.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: 1-arg form added. expinv_reg now defaults `mu=1` when only
  `p` supplied (was throwing "requires (p, mu)"). Spec covers default
  + non-default mu + p∈{0,1} boundaries + p out-of-range + mu<=0
  invalid. 5 TEST_F gtest + smoke .m. Parity OK numkit ↔ MATLAB ↔
  Octave at tol=1e-12.
