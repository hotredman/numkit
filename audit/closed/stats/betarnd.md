# stats.dist/betarnd — ТЗ for completion

**Status:** closed (vector-sz form; RNG-value parity deferred)
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
