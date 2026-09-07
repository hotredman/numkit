# lang.vm — dotted rvalue on a call result (`gcf.Number`) miscompiles to a dotted FUNCTION name in the VM; TreeWalker evaluates it correctly

- **Status:** 🔴 OPEN
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

## Root cause (suspected)

The compiler treats a FIELD_ACCESS whose head IDENTIFIER is not a known
variable as a qualified function name (the `graphics.layout.set`-style
dotted-call resolution) also in RVALUE position. For a dotted CALL with
parens that is right; for a bare dotted rvalue MATLAB semantics are
"evaluate the head (zero-arg call), then field-access".

## Suggested fix

In the dotted-name resolution: rvalue FIELD_ACCESS with a non-variable
IDENTIFIER head → emit a zero-arg call of the head + FIELD_ACCESS on the
result (or the TW behaviour). Keep the qualified-function path for actual
calls with parentheses.

## References

- **Guard:** `DISABLED_DottedRvalueOnCallResult` (dual-engine) in
  `src/bundle/tests/known_bugs_test.cpp`.
