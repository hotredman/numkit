# stats.dist/betarnd — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small (joint with `normrnd`)
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | rng-seeded reproducibility — different RNG from MATLAB | medium |
| 2 | `betarnd(a, b, [m n])` vector-sz form likely throws | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `rng(42); betarnd(2, 3)` | `0.4840964247` | `0.5501429068` (different RNG) |

## Recommended fixes

Joint with `audit/findings/stats/normrnd.md`. `betarnd` is built
from two `Gamma` variates (which derive from normal). Once the RNG
matches MATLAB at the normal level, beta inherits.

## Out of scope for this ТЗ

- N/A — joint fix.
