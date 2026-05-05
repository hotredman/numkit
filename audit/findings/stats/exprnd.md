# stats.dist/exprnd — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small (joint with `normrnd`)
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | rng-seeded reproducibility — different RNG | medium |
| 2 | `exprnd(mu, [m n])` vector-sz form likely throws | medium |
| 3 | `exprnd()` 0-arg or `exprnd` with default mu may have different defaults | low |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `rng(42); exprnd(2)` | `1.9641127107` | (different RNG value) |

## Recommended fixes

Joint with `audit/findings/stats/normrnd.md`.

## Out of scope for this ТЗ

- N/A.
