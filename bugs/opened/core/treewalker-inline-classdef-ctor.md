# core.treewalker — inline classdef: CONSTRUCTOR throws on the TreeWalker backend ("Cell contents indexing requires a cell array")

- **Status:** 🔴 OPEN
- **Kind:** bug
- **Severity:** P2 missing feature on one backend (TW) / P1-class impact contained by VM default
- **Found:** 2026-09-06 via the dual-engine guard added for the clear-all fix (`ClearSemanticsTest.InlineObjectWithoutClearControl/1`)

## Symptom

Constructing the `inline` object throws on the TreeWalker backend. The VM
(the default and the CLI/IDE backend) constructs it fine, so the
fidelity-pass tests (StrfunTest, VM-only fixture) never saw it.

## Repro

```matlab
clear;
g = inline('2*t', 't');
% numkit TW backend: "Cell contents indexing requires a cell array"
% numkit VM backend: g is a 1x1 inline object (class(g) = 'inline', g(3) = 6)
% MATLAB R2025b: g is a 1x1 inline object
```

(Repro needs the TW backend — e.g. `Engine::setBackend(Backend::TreeWalker)`
in a test; the CLI/IDE always run the VM, so users don't hit it today.)

## Root cause

Not yet diagnosed. The throw happens INSIDE the constructor invocation
(`g = inline('2*t','t')` itself), so it is the TW classdef-constructor
dispatch path — likely the `varargin` packing for the ctor call or the
method-body brace-indexing of a packed argument list differs on TW.
The classdef source is `kInlineClassSource` in
`src/bundle/src/register/builtin/math_integration_reg.cpp` (ctor takes
`varargin`; `subsref` calls `feval(obj.fh, s(1).subs{:})`).

## Suggested fix

Diagnose `TreeWalker::execCall` → class-ctor dispatch (invokeClassCtor /
invokeClassMethod) vs the VM's CALL handling for the same classdef;
compare how `varargin` reaches the ctor body on both. Likely a TW-side
packing fix in one place, not an inline-specific workaround.

## References

- Guard: `DISABLED_InlineCtorTreeWalker` in `src/bundle/tests/known_bugs_test.cpp`
- Dual-engine control that found it: `src/bundle/tests/clear_semantics_test.cpp`
  (inline cases are VM-only there, commented with this bug id)
- `src/core/src/tree_walker.cpp` (execCall / invokeClassMethod),
  `src/core/src/engine.cpp` (registerClassDef / invokeClassCtor)
