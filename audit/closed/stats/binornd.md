# stats.dist/binornd — ТЗ for completion

**Status:** closed (vector-sz form; RNG-value parity deferred)
**Priority:** low
**Effort:** small
**Audited at commit:** 1525319
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly across
all probed inputs (including `*rnd` under `rng(42)` — discrete
RNG appears to match MATLAB bit-for-bit, unlike the continuous
RNG family).

## Recommended fixes

1. **Spec extension** — fingerprint covering parameter sweeps +
   edge cases. `tol = 0` (integer-stable for discrete).

## Out of scope for this ТЗ

- N/A.

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
