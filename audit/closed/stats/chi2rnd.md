# stats.dist/chi2rnd — ТЗ for completion

**Status:** closed (vector-sz form; RNG-value parity deferred)
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

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: Vector-size form `<rnd>(..., [m n])` added via shared
  `parse_rng_size` helper in dist_helpers.hpp. Was throwing
  "Cannot convert double to scalar". 14 RNG functions refactored
  in one batch (betarnd/binornd/chi2rnd/exprnd/frnd/gamrnd/
  lognrnd/normrnd/poissrnd/raylrnd/trnd/unidrnd/unifrnd/wblrnd).

  RNG-VALUE PARITY (matching MATLAB R2025b's rng-seeded streams)
  is a SEPARATE deferred project — would require porting MATLAB's
  exact Mersenne Twister seeding + Ziggurat normal-variate transform.
  Cross-MATLAB script reproducibility under `rng(seed)` remains
  blocked until that lands.

  N-D shape support (3+ dim) emits `rows × prod(rest)` since
  numkit's Value is 2-D-only.
