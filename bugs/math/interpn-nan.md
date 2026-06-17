# interpn — 1-D grid-vector query returns NaN (and 4+-D unimplemented)

- **Status:** 🔴 OPEN
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
- **1-D** `interpn(x, v, xq)` — **broken** (returns NaN). ← this bug.
- **2-D / 3-D** — work (dispatch to interp2 / interp3); e2e
  `interpn-bug31.spec.js` pins both.
- **4+-D** generic tensor-product linear interp over 2^N corners per query —
  **not implemented** (backlog; a parity gap, see PARITY_GAPS.md).

## Where
`toolboxes/builtin/src/math/interp/interp.cpp` `interpn_reg` (registered in
`toolboxes/builtin/src/library.cpp`). The ndim-dispatch likely falls through to a
NaN-filled result for ndim == 1 (no interp1 delegation) and for ndim >= 4.

## Suggested fix
Delegate ndim == 1 to the existing `interp1` (linear) path, and implement the
generic N-D tensor-product linear interpolation for ndim >= 4 (2^N corner
weights per query point). Validate against MATLAB `interpn`.

## Guard
`toolboxes/builtin/tests/known_bugs_test.cpp` → `BuiltinKnownBug.DISABLED_InterpnOneDimNaN`
(asserts the MATLAB-correct 6.5; flip the prefix when fixed).
