# lang — multi-output call of a function-handle variable fails on the VM

- **Status:** 🔴 OPEN
- **Severity:** P2 (core VM limitation; blocks multi-output handle callbacks)
- **Kind:** bug
- **Found:** 2026-06-19 while implementing `fmincon` (its `nonlcon` interface
  is `[c, ceq] = nonlcon(x)`)

## Symptom
Calling a function handle held in a **variable** with **more than one output**
fails: the VM resolves the variable name as a *function name* and reports it
undefined, instead of making an indirect multi-output call. Single-output
handle calls work fine.

## Repro
```matlab
h = @(x) deal(x+1, x-1);
[a, b] = h(5);            % VM: "undefined function 'h' (in call to 'h')"
[a, b] = feval(h, 5);     % VM: "Too many output arguments. (in call to 'feval')"
r = h(5);                 % OK (single output)
v = @(x) [x+1, x-1];  r = v(5);   % OK — single output returning a vector
```
MATLAB: `[a, b] = h(5)` → `a = 6, b = 4`.

## Root cause
The VM's indirect-call path (CALL_INDIRECT on a handle variable) does not
support `nargout > 1`: with a bracketed LHS `[a, b] = h(...)` the dispatcher
falls back to name-based function resolution (so the variable name is looked
up as a builtin/user function and not found), and `feval` caps a handle at a
single output. The TreeWalker may differ — verify both backends.

## Impact
Blocks any `.m` / user code that calls a handle for several outputs — e.g.
`fmincon`'s `nonlcon` (`[c, ceq] = nonlcon(x)`), and generally
`[a, b, ...] = fh(args)` for a stored handle `fh`. `fmincon` currently
**rejects** a non-empty `nonlcon` because of this (see
[bugs/optim/fmincon.md](../optim/fmincon.md)).

## Suggested fix
Route a bracketed-LHS call whose callee is a handle-valued variable through
the indirect-call opcode with the requested `nargout`, and lift `feval`'s
single-output cap for handle targets. Add coverage for `[a,b]=h(x)`,
`[a,b]=feval(h,x)`, and the anonymous-`deal` form. Core VM change — touches
the bytecode call lowering + `feval`.

## References
- discovered in `src/bundle/src/register/optim/fmincon_reg.cpp`
- VM call lowering (CALL_INDIRECT), `feval` builtin
- guard test: `src/bundle/tests/known_bugs_test.cpp`
  (`DISABLED_MultiOutputHandleCall`)
