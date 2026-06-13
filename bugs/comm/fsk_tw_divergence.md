# TreeWalker `x(:)` on a row vector returns a row, not a column (surfaced via fsk round-trip)

- **Status:** ✅ FIXED (2026-06-13)
- **Severity:** P2 (general core indexing-semantics divergence; masked in most code, but `x(:)` is fundamental)
- **Kind:** bug
- **Found:** 2026-06-13 via DualEngineTest while adding comm coverage (CommModulationTest)
- **Real category:** core / TreeWalker indexing (NOT comm — fskmod/fskdemod are fine on both backends; this file kept at its original path so the spawned-task reference resolves).

## Fixed

2026-06-13. Added a core `Value::reshape(rows, cols, mr)` primitive
(`src/value/src/value.cpp`, decl in `value.hpp`) and routed the whole-array
colon `x(:)` (an empty `COLON_EXPR`) in `TreeWalker::execIndexAccess` through it,
forcing a `numel × 1` column for any source orientation — matching the VM and
MATLAB. Live guards: `CommModulationTest.FskRoundTrip` (un-DISABLED) +
`MatlabParity.ColonLinearIndex{RowVectorIsColumn,MatrixIsColumn}`. Full
desktop-fast suite green (11796 passed / 1 skipped, TW + VM).

## Symptom

On the **TreeWalker** backend, the whole-array colon index `x(:)` does **not**
reshape a **row** vector to a column — it returns the row unchanged. MATLAB (and
the numkit **VM**) always make `x(:)` a column (`numel × 1`). Matrices and
column vectors are reshaped correctly; only the row-vector case is wrong, which
is why it stayed hidden (the full suite is green).

The fsk round-trip merely *exposed* it: `fskdemod` returns an `N×1` column while
the comparison input `data` is a `1×N` row, so `out(:) - data(:)` is the first
place the orientation mismatch matters.

## Repro

Minimal (no comm involved):

```matlab
data = [1 2 3 4];
size(data(:))     % VM: [4 1]  (correct);  TreeWalker: [1 4]  (BUG)
```

Original symptom (fsk):

```matlab
data = [0 1 2 3 0 2 1 3];
out  = fskdemod(fskmod(data,4,100,10,2000), 4,100,10,2000);   % 8x1 column, OK on both
e    = sum(abs(out(:) - data(:)));
% VM:        out(:)=8x1, data(:)=8x1 -> diff 8x1 -> e = 0   (scalar)
% TreeWalker: out(:)=8x1, data(:)=1x8 -> diff broadcasts 8x8 -> sum -> 1x8 (NON-scalar)
%             -> downstream toScalar() throws "Cannot convert double to scalar"
```

## Root cause

`TreeWalker::execIndexAccess` (`src/core/src/tree_walker.cpp`, the `nargs == 1`
branch ~line 2080): the magic colon `:` (an empty `COLON_EXPR` node) is resolved
by `resolveIndex` to the full linear index list `[0 .. numel-1]`, then read via
`var.indexGet(indices, count)`. `indexGet` shapes the result after the **source**
orientation (row source → row result), so `rowvec(:)` stays a row. The `x(:)`
operator must instead force a `numel × 1` column regardless of source shape. The
VM handles this correctly — mirror it.

## Suggested fix

In `TreeWalker::execIndexAccess`, `nargs == 1`: detect the whole-array colon
(`callNode->children[1]` is `NodeType::COLON_EXPR` with empty `children`) and
return the result as a column vector (`numel × 1`), matching the VM and MATLAB.
Needs a core/value-level column reshape (`Value` has `objectReshape` for object
arrays; numeric/char/logical/cell/complex need the equivalent — either add a
small `Value` reshape primitive or build the column from the `indexGet` result).
Keep the general linear-index case `x(idx)` (which follows the index shape)
unchanged.

## References

- Live (disabled) guard: `CommModulationTest.DISABLED_FskRoundTrip` in
  `toolboxes/comm/tests/comm_modulation_test.cpp` — runs under
  `--gtest_also_run_disabled_tests` and fails on the `/TW` param.
- Add a direct regression for the minimal repro (`size(rowvec(:))`) under
  DualEngineTest when fixing.
- fskmod/fskdemod themselves are correct (parity specs `fskmod.json` /
  `fskdemod.json`, validated on the VM/default backend).
