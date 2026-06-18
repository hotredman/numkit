# interpn — 1-D grid-vector query returns NaN (and 4+-D unimplemented)

- **Status:** ✅ FIXED (2026-06-18) — 1-D grid-vector query `interpn(X,V,Xq)` delegates to interp1
- **Severity:** P2
- **Kind:** bug
- **Found:** 2026-05-03 (migrated from the old flat BUGS.md #31 when that log
  was retired; re-verified 2026-06-06)

## Symptom
`interpn` returns NaN for the simplest 1-D grid-vector form, instead of
interpolating. The 2-D / 3-D forms dispatch correctly (to interp2 / interp3,
per a 2026-05-10 partial fix), but the 1-D path and the generic N-D (4+-D
tensor-product) path do not work.

## Repro (numkit vs MATLAB R2025b)
```matlab
interpn([1 2 3], [1 4 9], 2.5)
% numkit:  NaN NaN NaN
% MATLAB:  6.5            (linear interp of v=[1 4 9] at xq=2.5)
```

## Status of the family
- **1-D** grid-vector query `interpn(X, V, Xq)` — **✅ FIXED** (delegates to interp1).
- **2-D / 3-D** — work (dispatch to interp2 / interp3); e2e
  `interpn-bug31.spec.js` pins both.
- **`interpn(V)` / `interpn(V, ntimes)`** grid REFINEMENT — still not implemented.
  This is a distinct MATLAB form (refines the grid; NOT a query — `interpn([1 4 9],
  2.5)` returns a 1×9 refined grid in MATLAB, not 6.5), so it is deliberately not
  routed to interp1. Separate parity gap.
- **4+-D** generic tensor-product linear interp over 2^N corners per query —
  **not implemented** (backlog; a parity gap, see PARITY_GAPS.md).

## Where
`src/math/src/interp/interp.cpp` `interpn_reg` (registered in
`src/bundle/src/register/math/interp_reg.cpp`). The ndim-dispatch likely falls through to a
NaN-filled result for ndim == 1 (no interp1 delegation) and for ndim >= 4.

## Fix (2026-06-18)
`interpn_reg` now detects the 1-D case — no argument is a 2-D+ array (for N>=2 the
value array V is itself N-D, so a matrix arg is always present) — and, when there
are >=3 leading data args (grid, values, query), delegates to `interp1` (whose
`(x, v, xq[, method[, extrap]])` spelling matches Form B exactly). 2-D/3-D
dispatch unchanged. `interpn([1 2 3],[1 4 9],2.5)` now returns 6.5.

**Caught by the parity cross-check** (an initial attempt also routed the 2-arg
`interpn(V, Xq)` to interp1 as 6.5 — but MATLAB reserves `interpn(V, scalar)` for
the grid-REFINEMENT form `interpn(V, ntimes)`, which returns a refined grid, not a
query). So only the >=3-data-arg grid-vector form is delegated; refinement + 4+-D
remain parity gaps. Parity OK vs MATLAB R2025b (tools/parity/specs/interpn.json).

## Guard
`src/bundle/tests/known_bugs_test.cpp` → `BuiltinKnownBug.InterpnOneDimNaN`
(promoted live, was `DISABLED_`) + `src/math/tests/interp_test.cpp` →
`InterpTest.Interpn1DGridVectorFormB` / `Interpn1DVectorQueryWithMethod`.
