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
- **P2 (done)** — **constructor + super-calls VM-native** (in-bytecode).
  - Super-calls: `SUPERCLASS_REF` now compiles (`Compiler::compileSuperCall`) to
    two new opcodes — `CALL_SUPER_CTOR` (`obj = obj@Base(args)`, lhs is a var) and
    `CALL_SUPER_METHOD` (`method@Base(obj,…)`, lhs is a method name), routed from
    both `compileCall` (single-output) and `compileMultiAssign` (multi-output).
    The throw-guard + uncompilable-fallback for super-calls is gone, so a method
    body containing a super-call now VM-compiles and runs as a frame.
  - Constructor: the VM `CALL` handler, when it resolves a class name, enforces
    ctor access (`Engine::enforceCtorAccess`), then — if the class has a
    compilable user ctor — pushes a VM frame for the ctor body, seeding the
    output variable with the default instance (`Engine::makeDefaultInstance`,
    `Engine::classCtor`, `pushCallFrame(..., ctorSeed)`). No user ctor / not
    VM-compilable → the C++ path (default-fill or TW hook). Covers both
    `ClassName(args)` and the bare-name `x = ClassName` (both compile to `CALL`).
  - The super-call sub-target (the *base* ctor/method) still delegates to TW via
    `superConstruct`/`superMethod` (a C++-initiated call → P3); the *calling*
    body runs on the VM and is debuggable.
  - **Proven**: `DebugSessionTest.BreakInsideClassdefSuperMethod` (breakpoint on
    the line after a super-method call pauses) and
    `BreakInsideClassdefConstructor` (breakpoint inside a ctor body pauses; the
    seeded output var is modified and returned). Full suite 10923 green.
- **P3 (done)** — **re-entrant VM-call-from-C++ foundation** + first consumer.
  - `VM::callReentrant(chunk, args, nargout, ownerClass, isCtor, ctorSeed)`:
    runs a compiled chunk as a fresh top-level frame and harvests all return
    values, then restores the outer VM state. Built on the same save/restore
    machinery as `execute()` — which is what makes it safe: `savePausedState`
    parks the `frames_` buffer and `restorePausedState` move-assigns it back, so
    the outer dispatch loop's `frame`/register references (which are NOT reloaded
    after a re-entrant builtin) stay valid. Unlike `execute()` it does not reset
    the debug controller, so callee frames nest on the live debug stack;
    `lastVarMap_` is snapshot/restored so the nested top-level export doesn't
    clobber the outer's pending workspace export.
  - First consumer: `Engine::callFunctionHandleMulti` routes user-function
    handles (named + anon; captures arrive as appended args) through
    `callReentrant` when the active backend is the VM — so `cellfun`/`arrayfun`/
    `feval` callbacks run on the VM, not the TreeWalker. Handle bodies invoked
    through a handle *variable* (`h(x)` → `CALL_INDIRECT`) were already
    same-stack VM frames (pausable); P3 covers the C++-initiated builtin path.
  - **Limitation (documented contract):** a debugger *pause* cannot suspend
    across the C++ re-entry boundary — a breakpoint reached inside a
    C++-initiated callback fires and then surfaces as `DebugStopException`
    (same as legacy `execute()`), it cannot freeze-and-resume the outer session.
    The pausable callback paths are the in-bytecode ones (`CALL_INDIRECT`, and
    P4's `FIELD_GET`/operator/`INDEX_*` opcode frame-pushes).
  - **Proven**: `DebugSessionTest.BreakInsideFunctionHandleCall` (breakpoint
    inside a handle-invoked function pauses on the VM); 166 handle/cellfun/
    arrayfun/feval dual-engine tests green on the VM path; full suite 10924.
- **P4a (done)** — **`get.Prop` accessors VM-native (in-bytecode, pausable).**
  The VM `FIELD_GET` opcode, for an object whose class defines a `get.Prop`
  accessor, runs the accessor body as a SAME-STACK VM frame (pushCallFrame +
  enter_frame, like P1) rather than the C++ `propGet` hook → `invokeClassMethod`
  → TreeWalker. No save/restore (fast) and fully pausable. A getter returns
  exactly one value, so the frame's destReg is the `FIELD_GET` destination.
  Property `GetAccess` is enforced (`Engine::enforcePropGetAccess`) before the
  frame is pushed; plain stored properties keep the fast `propGet` path. New
  seams: `Engine::classGetter`/`classSetter`/`enforcePropGetAccess`/
  `enforcePropSetAccess`. **Proven**: `DebugSessionTest.BreakInsideClassdefGetter`
  (breakpoint inside `get.scaled` pauses; `y == 21`); full suite 10925.
  - **Remaining P4** (follow-up commits, each in-bytecode + pausable like P4a):
    `set.Prop` setters via `FIELD_SET` (needs the handle-setter return-arity
    handling: a no-output handle setter must NOT clobber the object register —
    use a scratch destReg / shared-state mutation); operator methods via the
    arithmetic/comparison opcodes; `subsref`/`subsasgn` via `INDEX_GET`/
    `INDEX_SET`. Plus a backend-aware `invokeClassMethod`/`invokeClassCtor`
    (→ `callReentrant`) as the net for genuinely C++-initiated callbacks (e.g.
    `disp(obj)` from a builtin, a `sort` comparator) — measure before routing
    hot in-bytecode paths through its save/restore.
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
  `SUPERCLASS_REF`. **DONE** — see Progress above.
- **P3. re-entrant VM-call-from-C++ foundation.** Nested run on `frames_`;
  verify run-loop re-entrancy. **DONE** — `VM::callReentrant` + function-handle
  consumer; see Progress above.
- **P4. accessors / operators / subsref + function handles on VM.** Route the
  C++-initiated paths through P3; `invokeClassMethod`/`invokeClassCtor`/
  `callFunctionHandleMulti` backend-aware. Debugger through all of it.

## Invariants

- TW backend behaviour unchanged throughout.
- Native builtin-class methods (containers.Map, table, …) keep the C++ hook.
- Every phase: build all presets affected, run the full gtest suite, 0
  regressions, dual-engine tests for the new path (including a debugger test
  that a classdef method pauses/steps under the VM).
