# lang — anonymous function does not forward nargout (needs varargout)

- **Status:** ✅ FIXED (2026-06-19) — core varargout + anonymous nargout forwarding
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
value comes back. MATLAB instead **forwards the call-site nargout** into the
body expression (the anonymous function is `varargout`-like).

## Progress (2026-06-19)
- **Multi-output handle DISPATCH** fixed (`CALL_INDIRECT_MULTI`) — named /
  user-function handles do `[a,b]=h(x)` (see
  [multi-output-handle-call](multi-output-handle-call.md)). So a **named**
  nonlcon `function [c,ceq]=mycon(x)` could already work.
- **Core `varargout`** fixed (`RET_VARARGOUT`, commit 7f4287a9) —
  `function varargout = f(...)` with `varargout{k}=v` returns a dynamic count
  driven by the caller's nargout. (Live guard `BuiltinKnownBug.Varargout`.)

## Remaining gap — dynamic CSL cell-range LHS `[c{1:n}] = call()`
The anonymous forwarding needs the synthetic body
`[varargout{1:nargout}] = expr`, but a **cell-range multi-assign target with a
runtime count** is unsupported:
```matlab
function varargout = fwd()
  [varargout{1:nargout}] = deal(7,8,9);   % "Cannot convert double to scalar (in cell indexing)"
end
```
The multi-assign treats `varargout{1:nargout}` as one scalar-indexed target
instead of expanding it to `nargout` outputs and requesting that many from the
RHS call (a **runtime-nargout** call + cell distribution). Implementing this
(or an equivalent targeted forward opcode) is the last link to anonymous
multi-output and hence `fmincon`'s `@(x) deal(c,ceq)` nonlcon.

## Suggested fix
Support `[c{idxRange}] = call(...)` (and `[c{1:nargout}] = ...`): expand the
cell-range LHS to a runtime count, call the RHS with that nargout, and write
the results into the cell elements. Then lower `@(params) expr` (body a call)
to `function varargout = __anon_N(params)  [varargout{1:nargout}] = expr; end`.

## Repro
```matlab
% (a) anonymous deal
[a,b] = (@(x) deal(x+1,x-1))(5)             % "Too many output arguments"
% (b) plain varargout (the root)
function varargout = g(v), varargout{1}=v+1; varargout{2}=v-1; end
[a,b] = g(5)                                 % "Too many output arguments"
```

## Fix (2026-06-19)
Two parts:
1. **core `varargout`** (`RET_VARARGOUT`, commit 7f4287a9) —
   `function varargout = f(...)` with `varargout{k}=v` returns a dynamic count
   (numFixed + numel(cell)) driven by the caller's nargout.
2. **anonymous nargout forwarding** — `compileAnonFunc` lowers
   `@(params) g(...)` (body a **global** function call) to
   `function varargout = __anon_N(params)  varargout = __nk_fwd_call__(nargout,
   'g', args...);  end`. The `__nk_fwd_call__(n, fname, args...)` builtin
   resolves `fname` the SAME way a direct call does (`findExternal`, so
   import/namespace-aware — toolbox functions like `median` resolve), calls it
   with `nargout = n`, and returns the `n` results in a `1×n` cell that
   `RET_VARARGOUT` expands. A **captured-handle / parameter** callee
   (`@(x) f(x)` where `f` is a handle) keeps the single-output `__result__ =
   expr` path (must NOT be name-resolved) — so function *composition* is
   unaffected.

Verified: `[a,b]=(@(x) deal(x+1,x-1))(5)` = 6,4; single `h(5)` = 6; captures
(`k=100; [p,q]=(@(x) deal(x+k,x-k))(5)` = 105,−95); `arrayfun(@(x) max(x,2),
…)` and anonymous composition unchanged. Full suite 12397/0 — no regressions.
Unblocks `fmincon`'s nonlcon (`@(x) deal(c, ceq)`) — see
[bugs/optim/fmincon.md](../optim/fmincon.md), now nonlcon-capable.

## References
- `src/core/src/compiler.cpp` (`compileAnonFunc`, `compileFunction`,
  `compileMultiAssign`), `src/core/src/vm.cpp` (RET_MULTI / nargout)
- enabled by: [multi-output-handle-call](multi-output-handle-call.md) (dispatch)
- consumer: [bugs/optim/fmincon.md](../optim/fmincon.md) (nonlcon)
- guard test: `src/bundle/tests/known_bugs_test.cpp`
  (`DISABLED_AnonymousMultiOutput`)
