# stats.dist/normrnd — ТЗ for completion

**Status:** closed (vector-sz form; RNG-value parity deferred)
**Priority:** **high**
**Effort:** medium
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/normal.cpp:114` (`normrnd`)
- Adapter: `libs/stats/src/distributions/normal.cpp:183` (`normrnd_reg`)
- Spec: `tools/parity/specs/normrnd.json`
- What works today:
  - `r = normrnd(mu, sigma)` — scalar
  - `r = normrnd(mu, sigma, n)` — n×n
  - `r = normrnd(mu, sigma, m, n)` — m×n
- Throws on vector-size form `normrnd(mu, sigma, [m n])`.

## MATLAB R2025b — actual behavior

Documented signatures (`help normrnd`):

- `r = normrnd(mu, sigma)` — same shape as `mu`/`sigma`
- `r = normrnd(mu, sigma, sz)` — `sz` is a vector of dim sizes
  (most idiomatic form: `normrnd(0, 1, [m n])`)
- `r = normrnd(mu, sigma, sz1, sz2, ..., szN)` — multi-dim
- `r = normrnd(mu, sigma, n)` — n×n

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `normrnd(0, 1, [2 3])` vector-sz | 2×3 random matrix | adapter calls `args[2].toScalar()` ⇒ throws "Cannot convert double to scalar" | **high** (idiomatic MATLAB form) |
| 2 | rng-seeded reproducibility | `rng(42); normrnd(0,1)` returns `-0.5382` | numkit's `rng(42); normrnd(0,1)` returns a different value (e.g., `0.5154`) — different RNG algorithm | **high** (cross-MATLAB scripts can't reproduce) |
| 3 | N-D form `normrnd(0, 1, sz1, sz2, sz3)` | N-D output | not supported | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `normrnd(0, 1)` after `rng(42)` | `-0.5382438937` | `+0.5154330697` (different RNG) |
| `normrnd(0, 1, 3)` size | `[3 3]` | `[3 3]` ✅ |
| `normrnd(0, 1, 2, 3)` size | `[2 3]` | `[2 3]` ✅ |
| `normrnd(0, 1, [2 3])` size | `[2 3]` | THROWS |

## Recommended fixes

1. **Accept vector-sz `args[2]`:** when `args[2]` is a vector
   (`numel > 1`), unpack into rows/cols. The current 3-arg path
   that does `n×n` from scalar should still work — branch on
   `args[2].isScalar()`.
2. **Match MATLAB's RNG algorithm:** MATLAB's default RNG is the
   Mersenne Twister `mt19937ar`. numkit appears to use a different
   default (or the same MT but with different state seeding). The
   reproducibility gap is in either:
   - the seeding convention (MATLAB seeds the MT array a specific
     way from a single integer, with a known initial state at
     `rng(seed)`), or
   - the normal-variate transform (Box-Muller vs Marsaglia polar
     vs Ziggurat — MATLAB uses Ziggurat for the default `randn`).
   Implementing Ziggurat with MATLAB's exact tables would close
   the gap.
3. **N-D shape support:** allow trailing positional dims after
   `args[3]`.
4. **Spec extension:** add fingerprint for `[m n]` form, deterministic
   seed expectation (after RNG matches MATLAB), and shape-only
   assertions for non-deterministic cases. `tol = 0` for shape;
   `tol` not applicable for unmatched-RNG values.

## Out of scope for this ТЗ

- The `randn`-vs-`normrnd` distinction (separate function).
- MATLAB also supports `normrnd(mu, sigma, sz1, sz2, ...)` form for
  3+ dim — covered by the N-D shape gap.

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
