# stats.dist/frnd — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small (joint with `normrnd`)
**Audited at commit:** e580a5c
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | rng-seeded reproducibility — different RNG | medium |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `rng(42); frnd(5, 10)` | `1.3655604166` | `0.4723412579` (different RNG) |

## Recommended fixes

Joint with `audit/findings/stats/normrnd.md`. F-variates derive
from two chi-squared variates; once chi2rnd matches MATLAB, this
inherits.

## Out of scope for this ТЗ

- N/A — joint fix.
