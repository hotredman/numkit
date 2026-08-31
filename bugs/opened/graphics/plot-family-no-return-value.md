# graphics.plot-family — `h = plot(...)` binds NOTHING: graphics builtins return no value; first use dies with a misleading "undefined function"

- **Status:** 🔴 OPEN
- **Severity:** P2 (works in MATLAB, refused in numkit; handle-using plotting
  code is the textbook norm — `h = stem(...); set(h(1), 'MarkerSize', 3)`)
- **Kind:** bug
- **Found:** 2026-08-31 via fieldtest portion 1 (mdadams book, example_10.m
  — the untriaged `absent-fn 'h'` in the report)

## Symptom

Assigning the return of any plotting builtin leaves the target variable
UNBOUND. The next use of the variable fails with a misleading
"undefined function" error (the call-position lookup, since no variable
exists).

## Repro (self-contained)

```matlab
clear;
n = 0:5;
h = stem(n, sin(n), 'filled', 'b');
disp(exist('h'))
set(h(1), 'MarkerSize', 3);
% numkit:  exist → 0; then Error: VM: undefined function 'h' (in call to 'h')
% MATLAB R2025b: exist → 2 (handle object); set(...) succeeds, no output
```

Same for `h = plot(n, sin(n))` (exist → 0 in numkit).

## Root cause

The graphics builtins are wired as statement-like commands: they emit the
figure-data payload but return nothing to the evaluator, so the
assignment RHS evaluates to "no value" and the LHS is never assigned.

## Suggested fix

Graphics builtins return a graphics handle Value (a scalar object id or a
handle array for multi-plot calls) that `set`/`get` accept; the IDE
payload emission stays as-is. Minimum viable: return a lightweight
handle token so assignment binds and `set(h(1), …)` accepts the no-op
path — exact set() option coverage is separate.

## References

- **Guard:** `DISABLED_PlotFamilyBindsReturnHandle` in
  `src/graphics/tests/figure_test.cpp`.
