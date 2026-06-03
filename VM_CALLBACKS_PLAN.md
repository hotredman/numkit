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
6. **Debugger** — stepping/breakpoints must work inside these frames.
7. **Coexistence** — TW backend unchanged; native builtin-class methods keep
   the C++ hook.

## Phases (each = its own commit, full suite must stay green — 0 regressions)

- **P1. classdef instance methods VM-native** (in-bytecode). Compile bodies to
  global chunks; `methodChunk` map on `BuiltinClass`; `CALL`(function-form),
  `CALL_METHOD`, `CALL_METHOD_MULTI` enter a frame for classdef methods.
  Debugger test proving the body runs on the VM.
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
