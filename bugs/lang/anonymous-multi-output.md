# lang — anonymous function does not forward nargout (needs varargout)

- **Status:** 🔴 OPEN
- **Severity:** P2 (blocks anonymous multi-output handles; e.g. fmincon nonlcon)
- **Kind:** bug
- **Found:** 2026-06-19 (after fixing
  [multi-output-handle-call](multi-output-handle-call.md) — the dispatch works
  now, but the anonymous-function body has only one declared output)

## Symptom
An anonymous function called for several outputs throws *"Too many output
arguments"* even when its body produces them:
```matlab
h = @(x) deal(x+1, x-1);
[a, b] = h(5);            % numkit: "Too many output arguments"
% MATLAB: a = 6, b = 4
```
(Multi-output calls to **named** / **user-function** handles already work —
see [multi-output-handle-call](multi-output-handle-call.md).)

## Root cause
The compiler lowers `@(params) expr` to a synthetic function
`function __result__ = __anon_N(params)  __result__ = expr;  end` — a **single
declared return**. When the handle is called with `nargout = 2`, the body
`__result__ = deal(...)` evaluates `deal` with `nargout = 1`, so only one
value comes back and the second output is unset. MATLAB instead **forwards
the call-site nargout** into the body expression (the anonymous function is
`varargout`-like).

Underlying gap: numkit has **no `varargout` / dynamic-nargout** support at all
— a plain `function varargout = f(...)` with `[a,b]=f(...)` also throws "Too
many output arguments". There is no `varargout` handling in the compiler or
VM.

## Repro
```matlab
% (a) anonymous deal
[a,b] = (@(x) deal(x+1,x-1))(5)             % "Too many output arguments"
% (b) plain varargout (the root)
function varargout = g(v), varargout{1}=v+1; varargout{2}=v-1; end
[a,b] = g(5)                                 % "Too many output arguments"
```

## Suggested fix
Implement `varargout`:
- compiler: recognise `varargout` as the trailing return; lower
  `varargout{k}` writes and a `varargout`-returning function to a
  cell-collected, dynamic-count `RET_MULTI` driven by the runtime `nargout`;
- VM: distribute the dynamic return count to the caller's output registers
  (capping at the requested `nargout`).
Then lower `@(params) expr` whose body is a call to forward `nargout`
(`[varargout{1:nargout}] = expr`). Sizeable core change (compiler + VM).
Unblocks anonymous multi-output handles and hence `fmincon`'s `nonlcon`
(`@(x) deal(c, ceq)`), plus `feval(h, x)` for >1 output.

## References
- `src/core/src/compiler.cpp` (`compileAnonFunc`, `compileFunction`,
  `compileMultiAssign`), `src/core/src/vm.cpp` (RET_MULTI / nargout)
- enabled by: [multi-output-handle-call](multi-output-handle-call.md) (dispatch)
- consumer: [bugs/optim/fmincon.md](../optim/fmincon.md) (nonlcon)
- guard test: `src/bundle/tests/known_bugs_test.cpp`
  (`DISABLED_AnonymousMultiOutput`)
