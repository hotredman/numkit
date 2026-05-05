# stats.dist/gamrnd — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small (joint with `normrnd`)
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | rng-seeded reproducibility — different RNG | medium |
| 2 | `gamrnd(a, b, [m n])` vector-sz form likely throws | medium |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `rng(42); gamrnd(2, 1)` | `3.0556676770` | `1.9233702386` (different RNG) |

## Recommended fixes

Joint with `audit/findings/stats/normrnd.md`. Match MATLAB's
Marsaglia-Tsang gamma sampler.

## Out of scope for this ТЗ

- N/A — joint fix.
