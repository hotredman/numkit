# runtime.clear — `clear all/classes/functions` permanently wipes engine-startup builtin m-source functions and the inline classdef

- **Status:** ✅ FIXED (2c5a18370, 2026-09-06)
- **Kind:** bug
- **Severity:** P1 wrong result (functions become undefined / mis-dispatch after `clear all`)
- **Found:** 2026-09-06 via springer-math book scripts (pr1_1–pr1_3, pr2_4 all fail although the functions were implemented in the 2026-09-05 fidelity pass)

## Symptom

Any script that starts with `clear all` (the MATLAB convention — every book
example does) loses every engine-startup-registered m-source function and the
`inline` classdef for the REST of the session:

- `inline(...)` → "VM: undefined function 'inline'"
- `impulse(b,a,t)` → falls through to the control LTI path: "control response:
  expected an LTI struct"
- same for ode45/ode23, quadprog, fmincon, fminunc, fsolve, linprog, funm,
  decomposition, integral (19 m-source registrations + 1 classdef).

In MATLAB, `clear all` unloads compiled functions/classes from memory but they
reload TRANSPARENTLY from the toolbox installation on the next call. numkit's
startup registrations have no on-disk backing in the session, so once
`clearUserFunctions()`/`clearClassDefs()` removes them they are gone forever.

## Repro

```matlab
clear;
f = inline('2*t', 't');
clear all;
g = inline('2*t', 't');
% numkit (R2025b-09-05 build): VM: undefined function 'inline'
% MATLAB R2025b: works — g is a 1x1 inline object
```

```matlab
clear; clear all; th = 0:.5:2; h = impulse([1], [1 3 2], th);
% numkit: control response: expected an LTI struct (m-source impulse gone,
%         resolution falls to the control-toolbox overload)
% MATLAB R2025b: h is the 1x5 analog impulse response
```

## Root cause

`src/runtime/src/workspace.cpp` (clear builtin), branch `all`/`classes`:
`clearUserFunctions()` + `clearClassDefs()` wipe `userFuncs_` / `classDefs_`
indiscriminately — including the engine-startup registrations from
`Engine::registerBuiltinMSource` (19 call sites) and the `inline` classdef
registered via `engine.evalSafe(kInlineClassSource)`
(`src/bundle/src/register/builtin/math_integration_reg.cpp:535`).
`clear functions` likewise. Constants already have the correct pattern:
`reinstallConstants()` re-pins them after the wipe.

## Suggested fix

Mirror `reinstallConstants()`: the Engine retains the SOURCE TEXT of every
startup registration (`registerBuiltinMSource` appends to a member list; the
inline classdef switches to a new `registerBuiltinClassSource` that parses and
registers CLASSDEF_DEF nodes directly — same `registerClassDef` call the
compiler makes) and a `reinstallBuiltinSources()` replays them right after
the clear paths wipe them. Replay is parser+registry work only (no VM run),
safe to call from inside the clear external. Variables still get cleared;
file-backed user functions/classes keep their existing reload-on-reference
behavior.

## References

- `src/runtime/src/workspace.cpp` (clear external, `all`/`classes`/`functions` branches)
- `src/core/src/engine.cpp:1954` `registerBuiltinMSource`; `engine.cpp:792` `clearClassDefs`
- `src/core/src/engine.cpp:104` `reinstallConstants` (the pattern to follow)
- Found via: springer-math--signals-and-systems pr1_1/pr1_2/pr1_3 (inline),
  pr2_4 (impulse) — all annotated `known: missing: inline` / `untriaged
  runtime-error` in `fieldtest/compare/groups/springer-math.txt`; the
  annotations were stale, the functions work without `clear all`.
