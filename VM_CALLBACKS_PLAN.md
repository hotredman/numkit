# VM-native callbacks — make VM a complete parallel engine

Status: **planned**, on `core-dev`. Owner: CORE.

## Problem

VM and TW must be two **independently complete** backends. Today they are
not: every path where **C++ calls back into user code** runs on the
TreeWalker, *regardless of the active backend*:

- `Engine::callFunctionHandleMulti` → `treeWalker_->callHandleMultiPublic`
  (function handles: `cellfun`, `arrayfun`, `feval`, `@f`) — `engine.cpp` ~1669.
- `Engine::invokeClassMethod` / `invokeClassCtor` → `treeWalker_->runClassMethod`
  / `runClassCtor` — classdef methods, constructors, `get.`/`set.` accessors,
  operator methods, `subsref`/`subsasgn`.

The debugger lives on the **VM** (`VM::startExecution`/`resumeExecution`,
pause/resume, breakpoints). So any user code reached through these C++ hooks
**cannot be debugged**, and OOP / higher-order code never gets VM speed under
the VM backend. This is the gap to close.

## Two call shapes (different mechanisms)

1. **In-bytecode calls** — `obj.m()`, `m(obj)`, `[a,b]=obj.m()` appear as
   `CALL` / `CALL_METHOD` / `CALL_METHOD_MULTI` opcodes inside a running chunk.
   Clean fix: resolve the method's compiled chunk and
   `pushCallFrame(...) + goto enter_frame` (a normal VM frame on `frames_`),
   instead of invoking the `BuiltinClass` C++ hook. No re-entrancy needed;
   fully debuggable (it is just another frame).

2. **C++-initiated calls** — a builtin/hook *called by the VM* needs to run
   user code mid-instruction: property accessors (`FIELD_GET`→`propGet`),
   operator methods (`tryObjectBinaryOp`), `subsref`/`subsasgn`
   (`INDEX_GET`/`INDEX_SET`), and function handles (`cellfun` et al.). These
   need a **re-entrant VM call from C++**: run a chunk to completion on a
   nested region of `frames_`, return the result, let the outer loop resume.

## Key mechanisms (validated in code)

- `Compiler::registerFunctionAs(name, funcDef)` compiles a `funcDef` AST into a
  `BytecodeChunk` and binds it (`compiler.cpp` ~3567). Re-entrant compile is
  already exercised by nested `function` defs.
  - **Caveat:** it uses `scriptDepth_` → registers `scriptLocal` when inside a
    script compile, which is wrong for classdef methods (they must persist with
    the class across evals). Need a **global** registration path (or promote).
- `VM::pushCallFrame(chunk, args*, nargs, destReg, nargout, isMulti, outBase, nout)`
  does a defensive copy of `args` (`vm.cpp` ~2379) — so a temporary
  `[self, args…]` buffer is fine for `obj.m()` where receiver and args are not
  contiguous. For `m(obj, …)` the args are already contiguous (`self == args[0]`).
- `enter_frame:` label (`vm.cpp` ~315) — the run loop re-loads frame/ip/R from
  `frames_.back()` each entry; need to confirm full re-entrancy for nested runs.
- `VM::execute(chunk, args, nargs)` — run-to-completion entry (legacy);
  `startExecution`/`resumeExecution` — debug-aware. Re-entrant nesting on
  `frames_` is the open design question for shape (2).

## Sub-problems to solve

1. **Compile scope** — classdef method/ctor chunks must be **global**
   (persist with the class), not script-local. Add a forced-global compile.
2. **Inherited-method chunk naming** — a method's chunk lives under its
   *declaring* class (`uf.name == "Decl>method"`). Dispatch has the *object's*
   class. Store, per class, `methodChunk[mname] = uf.name` (own + inherited,
   built post-merge) so dispatch can `findCompiledFunc` it.
2. **Discriminator** — classdef method (bytecode frame) vs native builtin-class
   method (`containers.Map`, C++ hook). `methodChunk` map present ⇒ classdef.
3. **Constructor seed** — the VM ctor frame must pre-bind the output var to the
   default instance (TW does this in `runClassCtor`).
4. **super-calls** — `SUPERCLASS_REF` is TW-only (the VM compiler rejects it);
   compile it to a `CALL` into `Base>member`.
5. **Re-entrancy** (shape 2) — a safe nested VM run for C++-initiated callbacks;
   `invokeClassMethod`/`invokeClassCtor`/`callFunctionHandleMulti` become
   backend-aware.
6. **Access context must become frame-associated.** Today the member-access
   stack (`Engine::classCtx_`) is pushed/popped by a synchronous C++
   `ClassCtxGuard` around the TW call (`invokeClassMethod`). A VM frame enters
   via `goto enter_frame` and unwinds later via `RET`, so a C++ RAII guard
   can't bracket it. The frame must carry its `ownerClass`: push `classCtx_`
   in `pushCallFrame` (for a classdef-method frame) and pop in the `RET`
   handler. Without this, private/protected access checks inside a VM-executed
   method body see an empty context and wrongly deny.
7. **Debugger** — stepping/breakpoints must work inside these frames.
8. **Coexistence** — TW backend unchanged; native builtin-class methods keep
   the C++ hook.

## Progress

- **P1a (done, `258b014f`)** — foundation: `ensureClassMethodCompiled` (global
  chunk), `BuiltinClass::methodFns`, `Engine::ensureClassMethodChunk`. Inert.
- **P1b (done, `09699913`)** — **function-form** `m(obj)` runs as a VM frame;
  frame-associated access context (`CallFrame::ownerClass`, read on demand);
  graceful fallback to the hook when a body can't VM-compile yet; access
  enforced via `Engine::enforceMethodAccess`. **Proven**: a breakpoint inside
  `go(obj)` pauses under the VM debugger (DebugSessionTest).
- **P1c (done, `3b8c360c`)** — dotted `obj.m()` + `[a,b]=obj.m()` run as VM
  frames ([self,args] buffer). Multi-output super-call guard added
  (compileMultiAssign rejects SUPERCLASS_REF → hook fallback). Proven:
  BreakInsideClassdefMethodDotted pauses under the VM. **Phase 1 complete** —
  every classdef method-call form (function-form, dotted, multi) is VM-native
  and debuggable; native builtin-class methods keep the hook; bodies with
  super-calls fall back to the TW hook until P2.
  - **Found + filed** (task #49, pre-existing, NOT P1c): a parameter named
    `i`/`j` inside a VM function frame resolves to the imaginary unit instead
    of the parameter. General VM identifier-resolution bug — surfaced via a
    test method param named `i`.
  Original two prerequisites (both resolved above — neither was a P1c dispatch
  bug):
  1. **Multi-output super-call guard.** `[a,b]=m@Base(obj)` — the VM
     multi-assign compiler does NOT reject `SUPERCLASS_REF` (only single-output
     `compileCall` does), so it mis-compiles to a call of the base *name*
     ("VM: undefined function 'Shape'"). Guard it (throw → ensureCompiled
     catches → hook fallback) until P2 lands super-calls in the VM.
  2. **`obj.prop(i) = v` inside a VM *function frame* throws "Not a double
     array".** Works at script scope and via the TW hook, but a method body
     compiled to a chunk and run as a frame mis-handles the compound
     property-element assign. A real latent VM bug (any function with an
     object param doing `o.p(i)=v`), independent of classdef — fix before
     routing prop-assigning methods to frames.

## Phases (each = its own commit, full suite must stay green — 0 regressions)

- **P1. classdef instance methods VM-native** (in-bytecode). Compile bodies to
  global chunks; `methodChunk`/`methodFns` on `BuiltinClass`; `CALL`
  (function-form, **done**), `CALL_METHOD`, `CALL_METHOD_MULTI` enter a frame
  for classdef methods. Debugger test proving the body runs on the VM.
- **P2. constructor + super-calls VM-native.** Ctor frame with seed; compile
  `SUPERCLASS_REF`.
- **P3. re-entrant VM-call-from-C++ foundation.** Nested run on `frames_`;
  verify run-loop re-entrancy.
- **P4. accessors / operators / subsref + function handles on VM.** Route the
  C++-initiated paths through P3; `invokeClassMethod`/`invokeClassCtor`/
  `callFunctionHandleMulti` backend-aware. Debugger through all of it.

## Invariants

- TW backend behaviour unchanged throughout.
- Native builtin-class methods (containers.Map, table, …) keep the C++ hook.
- Every phase: build all presets affected, run the full gtest suite, 0
  regressions, dual-engine tests for the new path (including a debugger test
  that a classdef method pauses/steps under the VM).
