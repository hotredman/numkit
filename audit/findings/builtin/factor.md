# builtin/factor — ТЗ for completion

**Status:** open
**Priority:** medium (PROGRESS notes `correctness=MISMATCH` on bench input)
**Effort:** small
**Audited at commit:** f82f380
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | basic `factor(60)` matches `[2 2 3 5]` ✓ but PROGRESS notes a MISMATCH on the bench input ("sum of #factors for 1..1000") — a specific N value diverges. Probably edge handling for N=1 (MATLAB returns 1, numkit may return [] or different). | medium |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `factor(60)` | `[2 2 3 5]` | identical ✅ |
| `factor(1)` | (probe needed) | (probe needed) |
| `factor(0)` | (probe needed) | (probe needed) |

## Recommended fixes

1. **Re-probe edge cases** N=0, N=1, N=2 (smallest prime), large
   primes, perfect squares — find the specific input that diverges
   on the parity bench.
2. **Spec extension** after the divergence is identified.

## Out of scope for this ТЗ

- N/A.
