# stats.dist/raylrnd — ТЗ for completion

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
| `rng(42); raylrnd(1)` | `1.0206851152` | `0.6744986058` (different RNG) |

## Recommended fixes

Joint with `audit/findings/stats/normrnd.md`. raylrnd uses
inverse-cdf sampling on `unifrnd`; once unifrnd matches MATLAB,
this inherits.

## Out of scope for this ТЗ

- N/A — joint fix.
