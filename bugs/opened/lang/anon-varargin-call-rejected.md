# lang.anon — calling an anonymous function declared with `varargin` fails at every arity ("Too many input arguments")

- **Status:** 🔴 OPEN
- **Severity:** P2 (core MATLAB idiom — callbacks, default-absorbing
  shims, adapter handles; any code using it dies)
- **Kind:** bug
- **Found:** 2026-08-31 while designing a graphics no-op shim for the
  fieldtest corpus (the shim idea was dropped — numkit executes graphics
  natively — but this engine defect surfaced on the way)

## Symptom

An anonymous function whose parameter list is `varargin` rejects every
call — with arguments AND without. Regular (named-parameter) anonymous
functions work; `varargin` in ordinary function files works; only the
anonymous+varargin combination is broken.

## Repro (self-contained)

```matlab
clear;
f = @(varargin) numel(varargin);
disp(f(1, 2))
disp(f())
% numkit:  Error: Too many input arguments for function '__anon_1'
%          (both calls fail)
% MATLAB R2025b: 2
%                0
```

Real-world shape it blocks (the shim that found it):

```matlab
plot = @(varargin) [];   % neutralise graphics for a compute-only run
x = 1:10; plot(x, x.^2); % MATLAB: fine; numkit: same error
```

## Root cause

The anonymous-function arity check does not treat `varargin` as
absorbing: the declared formal count (0 explicit params besides varargin)
is enforced against the call site instead of accepting anything ≥ 0.

## Suggested fix

In the anonymous-function call path: if the handle's parameter list ends
with varargin, accept any argument count ≥ number of explicit formals,
packing extras (or all, when zero formals) into varargin. Mirror the
ordinary-function varargin semantics.

## References

- **Guard:** `DISABLED_AnonVararginCallArity` in
  `src/core/tests/functions_test.cpp` (dual-engine; asserts 2-arg and
  0-arg calls).
