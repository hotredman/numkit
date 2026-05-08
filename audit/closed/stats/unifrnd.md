# stats.dist/unifrnd — ТЗ for completion

**Status:** closed (vector-sz form; RNG-value parity deferred)
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
