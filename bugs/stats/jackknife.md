# jackknife(fn, X) throws "function-handle invocation not yet supported"

- **Status:** ✅ FIXED (2026-06-14)
- **Severity:** P2 (documented function is non-functional for its only signature)
- **Kind:** stub
- **Found:** 2026-06-14 while closing the stats coverage gap (target-A: every public fn gtested)

## Fixed

2026-06-14. Rewrote `jackknife_reg`
(`src/bundle/src/register/stats/resample/resample_reg.cpp`) to do the
leave-one-out loop inline via `ctx.engine->callFunctionHandle`, exactly like
`bootstrp_reg`, reusing the same-file `resampleRows` helper. Vector inputs are
reshaped to a column (`Value::reshape`) so each element is one observation;
matrices keep rows-as-observations. Output is the `n x K` matrix of
leave-one-out statistics. Live guard `StatsKnownBug.JackknifeMean` (un-DISABLED)
covers both the vector means `[3.5 3.25 3 2.75 2.5]` and a 3x2 matrix case. The
dead C++ stub `stats::jackknife` is now unreferenced (left in place; safe to
drop in a later cleanup).

## Symptom

The primary (and only) documented call form throws:

```matlab
jackknife(@mean, [1 2 3 4 5])
% Error: jackknife: function-handle invocation not yet supported
%        (numkit:jackknife:nyi)
```

So `jackknife` cannot be used at all — there is no working branch.

## Root cause

`jackknife_reg` (`src/bundle/src/register/stats/resample/resample_reg.cpp:389`)
delegates straight to the C++ free function
`stats::jackknife(fn, X, mr)` in
`src/toolboxes/stats/src/resample/resample.cpp:98`, which is a pure stub:

```cpp
Value jackknife(const Value & /*fn*/, const Value & /*X*/, std::pmr::memory_resource *)
{
    throw Error("jackknife: function-handle invocation not yet supported", ...);
}
```

This is the *same* situation `bootstrp` was in — its C++ free function
(`resample.cpp:88`) is also a stub — **except** `bootstrp_reg` does the work
inline using `ctx.engine->callFunctionHandle(...)` and never touches the stub.
`jackknife_reg` was never given the equivalent inline implementation.

## Suggested fix (mirror `bootstrp_reg`)

Rewrite `jackknife_reg` to do leave-one-out inline, exactly like
`bootstrp_reg`:

1. `n` = number of observations. For a matrix, `n = size(X,1)` (rows are
   observations). For a vector (row or column), `n = numel(X)` — **confirm the
   row-vector convention + output orientation against MATLAB R2025b before
   coding** (MATLAB returns an `n×K` matrix; for `@mean` on a length-5 vector
   that is a `5×1` column).
2. For each `i in 1..n`, build the sample with observation `i` removed (reuse a
   row-slice helper like `resampleRows`), call the handle via
   `callFunctionHandle`, and stack the `1×K` results into the `n×K` output.
3. Drop the dead `stats::jackknife` stub (and consider dropping the equally
   dead `stats::bootstrp` stub) once nothing references them.

Optionally add a `JackknifeCallbackBuiltin` for VM pausability later, matching
`BootstrpCallbackBuiltin`; not required for correctness.

This is a **behaviour change**, so it is gated on user sign-off per the
project's "behaviour fixes are collaborative" rule.

## Live guard

`StatsKnownBug.DISABLED_JackknifeMean` in
`toolboxes/stats/tests/known_bugs_test.cpp` asserts the leave-one-out means
`[3.5; 3.25; 3; 2.75; 2.5]` (mathematically exact for `@mean`). Remove the
`DISABLED_` prefix when the fix lands.
