# stats.dist/trnd — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small (joint with `normrnd`)
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/students_t.cpp` (`trnd`)
- Spec: `tools/parity/specs/trnd.json`
- Shape control via positional `(nu, m, n)`.

## MATLAB R2025b — actual behavior

- `r = trnd(nu)`
- `r = trnd(nu, sz1, sz2, ...)` / `trnd(nu, sz)` — vector size

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | rng-seeded reproducibility — different RNG | medium |
| 2 | `trnd(nu, [m n])` vector-sz | likely throws | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `rng(42); trnd(5)` | `-0.4285191940` | `+0.5192665185` (different RNG) |

## Recommended fixes

Joint with `audit/findings/stats/normrnd.md`. Once normrnd matches
MATLAB's RNG, the chi2/t derivatives follow.

## Out of scope for this ТЗ

- N/A — joint with normrnd.
