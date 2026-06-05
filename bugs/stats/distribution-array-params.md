# (cross-cutting) distribution functions don't broadcast ARRAY parameters

- **Status:** 🔴 OPEN (in progress — broadcast landing family-by-family)
- **Severity:** P2 (errors where MATLAB broadcasts)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (vector-parameter sweep)

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
`libs/stats/src/distributions/dist_helpers.hpp`: `broadcast_dist2` (data +
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
- [~] **gamma** — gampdf / gamcdf done (cycle 30, 2026-06-05); gaminv pending
- [~] **beta** — betapdf / betacdf done (cycle 30, 2026-06-05); betainv pending
- [~] **chi2** — chi2pdf / chi2cdf done (cycle 30, 2026-06-05); chi2inv pending
- [ ] students_t — tpdf / tcdf / tinv
- [ ] fisher_f — fpdf / fcdf / finv
- [ ] rayleigh — raylpdf / raylcdf / raylinv
- [ ] weibull — wblpdf / wblcdf / wblinv
- [ ] lognormal — lognpdf / logncdf / logninv

Implementation note: closed-form PDFs broadcast via a scalar kernel +
`broadcast_dist2/3`; CDFs reuse the already-broadcasting `builtin::gammainc`
/ `betainc` on a broadcast-transformed `x` (and `a`/`b`/`k`). The INV
functions (gaminv/betainv/chi2inv) are deferred to a follow-up cycle —
they need the degenerate-quantile handling (gamma `a==0`→0, chi2 `k==0`→0)
threaded through the parameter broadcast. Each adapter BRANCHES: scalar
parameters keep the untouched (hoisted-`lgamma`) public-fn fast path; only
non-scalar parameters take the broadcast path.

Discrete + remaining families (bino / poiss / unid / geo / nbin / hyge / …)
follow once the continuous set is done. The umbrella
`DISABLED_DistributionArrayParams` gtest (which also checks `binopdf` /
`gampdf`) stays disabled until those families land; the completed families
are guarded live by `libs/stats/tests/dist_broadcast_test.cpp`.

Verified vs MATLAB R2025b (cycle 29): `normpdf(0,0,[1 2 4])`,
`normcdf([0 1 2],0,[1 2 4])`, `norminv([.1 .5 .9],0,[1 2 3])`,
`exppdf(1,[1 2 4])`, `expcdf(1,[1 2 4][,'upper'])`, `expinv(0.5,[1 2 4])`,
per-element bad-param → NaN, 2×2 shape, empty→empty, size-mismatch error.

## References
- `libs/stats/src/distributions/dist_helpers.hpp` (broadcast_dist2/3, dist_param)
- `libs/stats/src/distributions/{normal,exponential}.cpp` (kernels + adapters)
- `libs/stats/tests/dist_broadcast_test.cpp`, `tools/parity/specs/dist_broadcast.json`
- MATLAB `doc normpdf` etc. ("inputs ... must be the same size, or any can be
  a scalar")
- related lesson: betastat scalar-only (memory feedback_audit_no_gap_can_lie)
