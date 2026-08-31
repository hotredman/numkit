# lang.varargin — `varargin` was UNIMPLEMENTED engine-wide: any function declaring it died on calls (file functions AND anonymous handles)

- **Status:** ✅ FIXED (2026-08-31)
- **Severity:** P2 → raised on triage: the gap was engine-wide, not anon-only
- **Kind:** bug
- **Found:** 2026-08-31 via the fieldtest graphics-shim experiment (the shim
  idea was dropped — numkit executes graphics natively — but this surfaced)

## Symptom (as filed)

```matlab
clear;
f = @(varargin) numel(varargin);
disp(f(1, 2))
disp(f())
% numkit (before): Error: Too many input arguments for function '__anon_1' (both calls)
% MATLAB R2025b:  2 / 0
```

**Triage correction:** the original filing assumed ordinary function files
with varargin worked. They did not — `function n = vfun(varargin) … end;
vfun(1,2)` failed identically. `varargin` was absent from the engine
entirely (only toolbox registration shims referenced the name).

## Root cause

Both call paths (TreeWalker::callUserFunction / callUserFunctionMulti,
VM::pushCallFrame) enforced `nargs == params.size()` with no special-case
for a trailing `varargin`, and no packing of extras into a cell existed.

## Fix

All three sites: a trailing `varargin` parameter absorbs extra arguments
into a 1×N cell (empty 0×0 cell when there are none); explicit formals
before it remain required ("Not enough input arguments" when short, the
MATLAB message). `nargin` keeps reporting the total argument count.
Verified on both engines (guard) and end-to-end: native REPL + WASM CLI
after dist refresh.

## References

- **Guard (live):** `AnonVararginCallArity` in
  `src/core/tests/functions_test.cpp` — dual-engine; covers anon 0/2-arg,
  mixed explicit+varargin, file-function form, and the too-few-args error.
