# (cross-cutting) distribution functions don't broadcast ARRAY parameters

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (errors where MATLAB broadcasts)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (vector-parameter sweep)

## Fixed
- Fixed: 2026-06-05 over a self-paced multi-cycle sweep (cycles 29-38). ALL
  16 distribution families' `*pdf` / `*cdf` / `*inv` now broadcast ARRAY
  distribution parameters, matching MATLAB: a scalar expands, equal non-scalar
  sizes element-align, mismatched non-scalar sizes error ("Non-scalar
  arguments must match in size."), the output shape follows the non-scalar
  operand, and per-element domain is honoured (bad param → NaN, out-of-support
  → 0, boundary → ±Inf).
- Families: **continuous** normal, exponential, gamma, beta, chi2, students_t,
  fisher_f, rayleigh, weibull, lognormal (c29-34); **discrete** binomial,
  poisson, unid, geometric, negbin, hypergeom (c35-37).
- Mechanism (`toolboxes/stats/src/distributions/dist_helpers.hpp`):
  `broadcast_dist2` / `broadcast_dist3` / `broadcast_dist4` apply a scalar
  kernel under MATLAB broadcasting; `dist_param` resolves zero-copy defaults;
  `dist_match_numel` enforces the size rule. Closed-form pdf/cdf/inv use a
  scalar kernel; cdf/inv that compose incomplete beta/gamma reuse the
  already-broadcasting `math::betainc` / `gammainc` / `*incinv` on a
  broadcast-built argument plus a per-element fixup loop (x-sign for t,
  degenerate quantile for gamma/chi2, nu→∞ Gaussian limit for t, discrete
  quantile walks for the discrete families). Each adapter BRANCHES: scalar
  parameters keep the untouched (hoisted-`lgamma`) public-fn fast path; only
  non-scalar parameters take the broadcast path, so there is no perf or
  behaviour change to the common scalar case.
- Guards: `toolboxes/stats/tests/dist_broadcast_test.cpp` (36 TEST_F, one block per
  family), `tools/parity/specs/dist_broadcast.json` (correctness=OK vs MATLAB
  R2025b), `toolboxes/stats/tests/smoke/dist_broadcast_smoke.m`, and the now-live
  umbrella `StatsKnownBug.DistributionArrayParams`. Also retrofitted the
  `dist_match_numel` guard onto `betacdf`/`betainv` (the underlying
  `math::betainc`/`betaincinv` don't validate sizes and would OOB-read).

## Symptom
The `*pdf` / `*cdf` / `*inv` distribution functions vectorise the FIRST
argument (`x`) but require the distribution PARAMETERS to be scalars: passing
a vector/array for `mu`, `sigma`, `n`, `p`, `a`, `b`, `df`, … throws "Cannot
convert double to scalar". MATLAB broadcasts all arguments to a common size.

## Repro
```matlab
normpdf(0, 0, [1 2 4])
% numkit: Error — Cannot convert double to scalar
% MATLAB: [0.3989  0.1995  0.0997]
binopdf(2, [4 5 6], 0.5)
% numkit: Error
% MATLAB: [0.3750  0.3125  0.2344]
gampdf(1, [1 2 3], 1)
% numkit: Error
% MATLAB: [0.3679  0.3679  0.1839]
normpdf([0 1], [0 0], [1 2])   % all-vector broadcast
% MATLAB: [0.3989  0.1760]
normcdf(1, [0 1], 1)           % MATLAB: [0.8413  0.5]
norminv(0.5, [0 5], 1)         % MATLAB: [0  5]
```
A scalar parameter with a vector `x` works (e.g. `poisspdf([0 1 2], 2)`,
`normcdf([-1 0 1])`), so only the PARAMETER args are affected.

## Affected (probed; likely the whole family)
normpdf/normcdf/norminv, binopdf, gampdf, betapdf, unifpdf, tpdf — and almost
certainly every other `*pdf`/`*cdf`/`*inv` (exp, poiss-params, chi2, f, wbl,
lognorm, ev, nbin, …).

## Root cause
The distribution adapters read each parameter via `Value::toScalar()` (or
take a `double` parameter in the public signature), so a non-scalar parameter
trips the scalar conversion. Only `x` is looped element-wise.

## Suggested fix
Broadcast all arguments to a common size (scalar expands; equal non-scalar
sizes element-align; mismatched non-scalar sizes error like MATLAB), then
evaluate element-wise. Best done once in a shared helper that the
distribution adapters route through, rather than per-function. Moderate but
systemic — high value (vectorised pdf/cdf over parameter grids is common).

## Progress (multi-cycle — md stays OPEN until all families broadcast)

Shared broadcast helpers added in
`toolboxes/stats/src/distributions/dist_helpers.hpp`: `broadcast_dist2` (data +
1 param) and `broadcast_dist3` (data + 2 params), plus `dist_param`
(zero-copy default resolution). Each distribution supplies a scalar kernel
that owns its per-element domain (param<=0 / NaN → NaN), and the adapter
routes (x, params…) through the helper: scalar expands, equal non-scalar
sizes element-align, mismatched non-scalar sizes error
(`"Non-scalar arguments must match in size."`), output shape follows the
non-scalar operand. The scalar-parameter fast path stays bit-identical
(kernels mirror the old formulas exactly).

Continuous location-scale family:
- [x] **normal** — normpdf / normcdf / norminv (cycle 29, 2026-06-05)
- [x] **exponential** — exppdf / expcdf / expinv (cycle 29, 2026-06-05)
- [x] **gamma** — gampdf / gamcdf / gaminv (cycle 30 pdf+cdf, 31 inv)
- [x] **beta** — betapdf / betacdf / betainv (cycle 30 pdf+cdf, 31 inv)
- [x] **chi2** — chi2pdf / chi2cdf / chi2inv (cycle 30 pdf+cdf, 31 inv)
- [x] **students_t** — tpdf / tcdf / tinv (cycle 33, 2026-06-05; betainc-based, nu==Inf Gaussian limit per element)
- [x] **fisher_f** — fpdf / fcdf / finv (cycle 34, 2026-06-05; betainc-based, 2-param)

**All 10 continuous families broadcast (cycles 29-34).** Remaining: the
DISCRETE families below (integer-ish params, different domains).
- [x] **rayleigh** — raylpdf / raylcdf / raylinv (cycle 32, 2026-06-05)
- [x] **weibull** — wblpdf / wblcdf / wblinv (cycle 32, 2026-06-05)
- [x] **lognormal** — lognpdf / logncdf / logninv (cycle 32, 2026-06-05)

Implementation note: closed-form PDFs broadcast via a scalar kernel +
`broadcast_dist2/3`; CDFs reuse the already-broadcasting `math::gammainc`
/ `betainc` on a broadcast-transformed `x` (and `a`/`b`/`k`); INVs reuse the
broadcasting `math::gammaincinv` / `betaincinv` plus a per-element fixup
loop for the degenerate quantile (gamma `a==0`→0, chi2 `k==0`→0;
`a<0`/`b<=0`/`k<0`→NaN). Each adapter BRANCHES: scalar parameters keep the
untouched (hoisted-`lgamma`) public-fn fast path; only non-scalar parameters
take the broadcast path. `dist_match_numel` enforces MATLAB's "Non-scalar
arguments must match in size." (cycle 31 also retrofitted that guard onto
`betacdf`/`betainv`, since `math::betainc`/`betaincinv` don't validate
sizes and would otherwise read out of bounds).

Discrete families (pmf closed-form kernel; cdf via per-element
`betainc`/`gammainc`; inv = discrete-quantile walk per element; integer-ish
params → noninteger n/N/K → NaN, noninteger k → 0):
- [x] **binomial** — binopdf / binocdf / binoinv (cycle 35, 2026-06-05)
- [x] **poisson** — poisspdf / poisscdf / poissinv (cycle 35, 2026-06-05)
- [x] **unid** — unidpdf / unidcdf / unidinv (cycle 36, 2026-06-05; closed-form)
- [x] **geometric** — geopdf / geocdf / geoinv (cycle 36, 2026-06-05; closed-form)
- [x] **negbin** — nbinpdf / nbincdf / nbininv (cycle 37, 2026-06-05; betainc)
- [x] **hypergeom** — hygepdf / hygecdf / hygeinv (cycle 37, 2026-06-05; 4-operand broadcast_dist4)

**All 16 distribution families now broadcast array parameters (cycles
29-37).** Finale (enable umbrella test + flip md to FIXED) is the next cycle.

The umbrella `DISABLED_DistributionArrayParams` gtest (which checks
`normpdf` / `binopdf` / `gampdf` — all three now broadcast) stays disabled
until the remaining discrete families land, when it is enabled together with
the md flip to FIXED; the completed families are guarded live by
`toolboxes/stats/tests/dist_broadcast_test.cpp`.

Verified vs MATLAB R2025b (cycle 29): `normpdf(0,0,[1 2 4])`,
`normcdf([0 1 2],0,[1 2 4])`, `norminv([.1 .5 .9],0,[1 2 3])`,
`exppdf(1,[1 2 4])`, `expcdf(1,[1 2 4][,'upper'])`, `expinv(0.5,[1 2 4])`,
per-element bad-param → NaN, 2×2 shape, empty→empty, size-mismatch error.

## References
- `toolboxes/stats/src/distributions/dist_helpers.hpp` (broadcast_dist2/3, dist_param)
- `toolboxes/stats/src/distributions/{normal,exponential}.cpp` (kernels + adapters)
- `toolboxes/stats/tests/dist_broadcast_test.cpp`, `tools/parity/specs/dist_broadcast.json`
- MATLAB `doc normpdf` etc. ("inputs ... must be the same size, or any can be
  a scalar")
- related lesson: betastat scalar-only (memory feedback_audit_no_gap_can_lie)
