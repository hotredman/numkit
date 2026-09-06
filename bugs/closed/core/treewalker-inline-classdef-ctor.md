# core.treewalker — inline classdef: CONSTRUCTOR throws on the TreeWalker backend ("Cell contents indexing requires a cell array")

- **Status:** ✅ FIXED (490e57f08, 2026-09-06)
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

TreeWalker::runClassCtor bound constructor parameters POSITIONALLY
only: a ctor declaring `varargin` received the RAW first argument in
the `varargin` slot instead of a packed cell, so `varargin{1}` threw
"Cell contents indexing requires a cell array". (Plain methods went
through callUserFunctionMulti, which packs correctly — only the ctor
path lacked it.)

## Fix

runClassCtor now packs extras into a 1xN cell exactly like the regular
function-call path (and skips the too-many-args throw when varargin is
declared). Live guard: `TwBackendKnownBug.InlineCtorTreeWalker`; the
inline clear-semantics case is dual-engine again
(`Backends/ClearSemanticsTest.ClearAllKeepsInlineClassdef`).

## References

- Guard: `InlineCtorTreeWalker` (live) in `src/bundle/tests/known_bugs_test.cpp`
- Dual-engine control that found it: `src/bundle/tests/clear_semantics_test.cpp`
  (inline cases are VM-only there, commented with this bug id)
- `src/core/src/tree_walker.cpp` (execCall / invokeClassMethod),
  `src/core/src/engine.cpp` (registerClassDef / invokeClassCtor)
