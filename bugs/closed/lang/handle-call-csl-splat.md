# lang.calls — `f(c{:})` with a VARIABLE (function-handle) callee rejects the CSL ("Too many values")

- **Status:** ✅ FIXED (7587ff101, 2026-09-07)
- **Severity:** P2 (core MATLAB idiom; named callees work, handle callees don't)
- **Kind:** bug
- **Found:** 2026-09-05 while implementing the inline class (its subsref
  needs to splat `s(1).subs{:}` into a stored handle)

## Symptom

CSL-splatting into call arguments works for NAMED functions (the compiler
emits CALL_FLATTEN_MULTI) but not when the callee is a variable holding a
function handle.

## Repro (self-contained)

```matlab
clear;
f = @(a,b) a + b;
c = {2, 3};
disp(f(c{:}))
% numkit:  Error: Too many values: a comma-separated list expanded to 2
%          values where 1 is required
% MATLAB R2025b: 5
% (feval(f, c{:}) WORKS in numkit — the named-callee flatten path)
```

## Root cause

compiler.cpp ~1023: the CALL_FLATTEN(_MULTI) emission is gated on
`!calleeIsVar` — identifier callees only. A variable callee with brace
args compiles to the plain call path, where the CSL value arrives as one
argument and the arity check rejects it.

## Suggested fix

A CALL_INDIRECT_FLATTEN variant (or extend the flatten gate to variable
callees whose args contain CELL_INDEX), flattening at the
execCallIndirect boundary. TreeWalker needs the symmetric fix.

## References

- **Guard:** deferred — engine feature landing in the compiler + VM +
  TreeWalker simultaneously; the repro above is the guard pattern
  (f=@(a,b)a+b; f({2,3}{:}) == 5).
