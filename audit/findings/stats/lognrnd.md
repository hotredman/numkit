# stats.dist/lognrnd — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small (joint with `normrnd`)
**Audited at commit:** 105c2b4
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | rng-seeded reproducibility — different RNG | medium |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `rng(42); lognrnd(...)` | (different value from numkit) | — |

## Recommended fixes

Joint with `audit/findings/stats/normrnd.md`. Once base RNG
matches MATLAB, this inherits.

## Out of scope for this ТЗ

- N/A — joint fix.
