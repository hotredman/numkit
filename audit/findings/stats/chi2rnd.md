# stats.dist/chi2rnd — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small (joint with `normrnd`)
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/chi2.cpp` (`chi2rnd`)
- Spec: `tools/parity/specs/chi2rnd.json`
- What works today: shape control via positional `(k, m, n)`.

## MATLAB R2025b — actual behavior

- `r = chi2rnd(nu)`
- `r = chi2rnd(nu, sz1, sz2, ...)` / `chi2rnd(nu, sz)` — vector size

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | rng(seed) reproducibility — same seed gives different values from MATLAB | medium (can't reproduce MATLAB scripts bit-for-bit) |
| 2 | `chi2rnd(k, [m n])` vector-sz form | likely throws (same parser shape as normrnd) | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `rng(42); chi2rnd(3)` | `4.7528933292` | `2.8380840089` (different RNG) |
| `chi2rnd(3, 2, 3)` size | `[2 3]` | `[2 3]` ✅ shape OK |

## Recommended fixes

1. **Joint fix with `normrnd`:** match MATLAB's RNG (Mersenne
   Twister + Ziggurat for normals; chi2 derives from normal/gamma
   variates).
2. **Vector-sz form** — same parser fix as `normrnd`.
3. **Spec extension** — shape-only assertions for now; bit-
   reproducibility once RNG matches.

## Out of scope for this ТЗ

- N/A — joint with normrnd.
