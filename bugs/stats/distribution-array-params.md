# (cross-cutting) distribution functions don't broadcast ARRAY parameters

- **Status:** 🔴 OPEN
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

## References
- `libs/stats/src/distributions/*` (the *pdf/*cdf/*inv adapters)
- MATLAB `doc normpdf` etc. ("inputs ... must be the same size, or any can be
  a scalar")
- related lesson: betastat scalar-only (memory feedback_audit_no_gap_can_lie)
