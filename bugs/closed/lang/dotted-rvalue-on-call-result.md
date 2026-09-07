# lang.vm — dotted rvalue on a call result (`gcf.Number`) miscompiles to a dotted FUNCTION name in the VM; TreeWalker evaluates it correctly

- **Status:** ✅ FIXED (portion 30, commit 8902bde37)
- **Severity:** P3 (MATLAB-legal field access on a function's return value;
  workaround: assign to a temp variable first)
- **Kind:** bug
- **Found:** 2026-09-07 portion 29 — the dual-engine handle suite diverged:
  `GraphicsHandleTest.FigureReturnsHandleAndNumbersSequentially` passed on
  TW, failed on VM.

## Symptom

`fn.Prop` where `fn` is a FUNCTION (not a workspace variable) compiles in
the VM to a lookup of the dotted function name `"fn.Prop"` and dies with
`VM: undefined function 'gcf.Number'`. TreeWalker evaluates the same
expression the MATLAB way: call `fn`, then field-access the result.

## Repro (self-contained)

```matlab
clear;
f = figure();
g = gcf;
g.Number            % fine on both engines
try
  disp(gcf.Number)  % the dotted-rvalue form
  disp('ok')
catch e
  disp(['caught: ' e.message])
end
% numkit VM:  caught: VM: undefined function 'gcf.Number'
% numkit TW:  1   (figure number — correct)
% MATLAB R2025b: 1
```

## Root cause

The compiler treated a FIELD_ACCESS whose head IDENTIFIER is not a known
variable as a qualified function name (the `graphics.layout.set`-style
dotted-call resolution) also in RVALUE position: it emitted
`CALL "gcf.Number"` and the runtime threw. The TreeWalker walks the AST
directly — its execFieldAccess tries the qualified external first and
otherwise evaluates the head via execIdentifier (a 0-arg call), which is
why TW was correct.

## Fix (portion 30)

compileFieldAccess (src/core/src/compiler.cpp) gained the TW-mirroring
fallback: when NOTHING dotted is registered (neither the full qualified
name nor a ≥2-segment prefix) but the HEAD is a callable name
(Engine::hasCallableName: registered external, short-name leaf, or an
already-registered user function — no lazy m-file probing), it emits a
0-arg CALL of the head + FIELD_GET chain for the trailing segments.
Class heads are excluded (classdef Static/Constant members register the
full dotted name and are caught above; an unregistered drift must not
silently construct an instance). Package-like heads (`pkg.sub.fn`
m-files, lazily resolved at runtime) keep the qualified CALL.

Verified unchanged: MathUtil.Answer (class constants),
Weekday.Monday.num (enum chains), Static methods, struct fields,
import-qualified calls. Full Release suite 13276/13278-equivalent green.

## References

- **Guard:** `DottedRvalueOnCallResult` (dual-engine, enabled) in
  `src/bundle/tests/known_bugs_test.cpp`.
