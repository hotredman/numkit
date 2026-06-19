# lang — multi-output call of a function-handle variable fails on the VM

- **Status:** ✅ FIXED (2026-06-19) — new `CALL_INDIRECT_MULTI` opcode
- **Severity:** P2 (core VM limitation; blocks multi-output handle callbacks)
- **Kind:** bug
- **Found:** 2026-06-19 while implementing `fmincon` (its `nonlcon` interface
  is `[c, ceq] = nonlcon(x)`)

## Symptom
Calling a function handle held in a **variable** with **more than one output**
failed: the VM resolved the variable name as a *function name* and reported
it undefined, instead of making an indirect multi-output call. Single-output
handle calls worked.

## Repro
```matlab
h = @size;  [r, c] = h(ones(2,3));   % was: "undefined function 'h'"; now r=2, c=3
function [p,q] = f(v), p=v+1; q=v-1; end
g = @f;  [a, b] = g(10);             % now a=11, b=9
```

## Root cause
The VM's multi-output assignment path (`compileMultiAssign`) only emitted the
**name-based** `CALL_MULTI` for an IDENTIFIER callee — it never checked
whether that identifier was a local/workspace variable holding a handle
(unlike the single-output `compileCall`, which gates a `CALL_INDIRECT` on
exactly that). So `[a,b] = h(x)` looked `h` up as a function name and failed.

## Fix (2026-06-19)
Added a `CALL_INDIRECT_MULTI` bytecode opcode (`a=outBase, b=fhReg,
c=argBase, d=nargs, e=nout`) that mirrors `CALL_INDIRECT`'s handle resolution
(plain or closure-cell, with captures) and `CALL_MULTI`'s `nout` frame-push.
`compileMultiAssign` now applies the same known-variable gate as the
single-output path and emits `CALL_INDIRECT_MULTI` for a handle-valued
callee; the VM's `execCallIndirectMulti` dispatches it (compiled user /
anonymous function → multi-return frame push with `nargout = nout`; external
builtin → call with `nout` outputs). Full suite 12394/0 — no regressions.

Verified: `[r,c] = (@size)(ones(2,3))` → `2, 3`; a user `function [p,q]=f(v)`
via `g=@f` → `[a,b]=g(10)` = `11, 9`.

## Remaining sub-gap (separate bug)
An **anonymous** function does not forward `nargout` to its body, so
`h = @(x) deal(x+1,x-1); [a,b]=h(5)` still throws *"Too many output
arguments"* (the synthetic `__result__ = deal(...)` body has one declared
return). That needs **`varargout` / dynamic-nargout** support, which numkit
lacks entirely — filed as
[bugs/lang/anonymous-multi-output](anonymous-multi-output.md). `fmincon` keeps
rejecting `nonlcon` until that lands (the common form is `@(x) deal(c, ceq)`).
`feval(h, x)` for >1 output also still caps at one (same `varargout` root).

## References
- `src/core/include/numkit/core/bytecode.hpp` (`CALL_INDIRECT_MULTI`),
  `src/core/src/compiler.cpp` (`compileMultiAssign` gate),
  `src/core/src/vm.cpp` (`execCallIndirectMulti`)
- live guard: `src/bundle/tests/known_bugs_test.cpp` (`MultiOutputHandleCall`)
- remaining: [bugs/lang/anonymous-multi-output](anonymous-multi-output.md)
