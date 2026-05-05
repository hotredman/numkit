# stats.dist/unifrnd — ТЗ for completion

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
| `rng(42); unifrnd(0, 1)` | `0.3745401188` | `0.7965429843` (different RNG) |
| `rng(42); unifrnd(2, 5)` | `3.1236203565` | `4.3896289529` |

## Recommended fixes

Joint with `audit/findings/stats/normrnd.md`. The base uniform
RNG drives every other random function — fix here cascades to
all `*rnd` functions.

## Out of scope for this ТЗ

- N/A — joint fix.
